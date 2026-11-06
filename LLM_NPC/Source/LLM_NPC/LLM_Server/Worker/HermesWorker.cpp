#include "LLM_NPC/LLM_Server/Worker/HermesWorker.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "LLM_NPC/LLM_Server/HermesServerManager.h"
#include "LLM_NPC/LLM_Server/LLMRequestManager.h"

namespace
{
	struct FHermesHttpReqState
	{
		FLLMResponse      Response;
		std::atomic<bool> bAborted{ false };
		FEvent*           DoneEvent = nullptr;
	};
}

FHermesWorker::FHermesWorker(ALLMRequestManager* _requestManager)
	:m_requestManager(_requestManager)
{

}

bool FHermesWorker::Init()
{
	UE_LOG(LogTemp, Log, TEXT("[HermesWorker] Thread gestartet"));
	return true;
}

uint32 FHermesWorker::Run()
{
	while (!m_bShouldStop.load())
	{
		FLLMRequest Request;
		bool bHasRequest = false;

		{
			std::unique_lock<std::mutex> Lock(m_cvMutex);
			m_cv.wait(Lock, [this]()
			{
				FScopeLock q(&m_queueMutex);
				return !m_queue.empty() || m_bShouldStop.load();
			});

			if (m_bShouldStop.load()) break;

			FScopeLock q(&m_queueMutex);
			if (!m_queue.empty())
			{
				Request     = m_queue.top();
				m_queue.pop();
				bHasRequest = true;
			}
		}

		if (bHasRequest)
		{
			ProcessRequest(Request);
		}
	}
	return 0;
}

void FHermesWorker::Stop()
{
	m_bShouldStop.store(true);
	m_cv.notify_all();
}

void FHermesWorker::Exit()
{
	UE_LOG(LogTemp, Log, TEXT("[HermesWorker] Thread beendet"));
}

void FHermesWorker::EnqueueRequest(const FLLMRequest& _request)
{
	{
		FScopeLock Lock(&m_queueMutex);
		m_queue.push(_request);
	}
	m_cv.notify_one();
}

void FHermesWorker::ProcessRequest(const FLLMRequest& _request)
{
	FString formattedPrompt;
	// System
	formattedPrompt += FString::Printf(
		TEXT("<|im_start|>system\n%s\n<|im_end|>\n"),
		*_request.SystemPrompt
	);
	// Verlauf
	for (int32 i = 0; i < _request.HistoryRoles.Num(); i++)
	{
		formattedPrompt += FString::Printf(
			TEXT("<|im_start|>%s\n%s\n<|im_end|>\n"),
			*_request.HistoryRoles[i],
			*_request.HistoryContents[i]
		);
	}

	const bool bConstrained = !_request.JsonSchema.IsEmpty();
	formattedPrompt += FString::Printf(
		TEXT("<|im_start|>user\n%s\n<|im_end|>\n<|im_start|>assistant\n%s"),
		*_request.UserMessage,
		bConstrained ? TEXT("") : TEXT("{")
	);
	
	// Config aus HermesServerManager lesen
	UHermesServerManager* hermesMgr = nullptr;
	if (m_requestManager)
	{
		if (UGameInstance* gi = m_requestManager->GetGameInstance())
		{
			hermesMgr = gi->GetSubsystem<UHermesServerManager>();
		}
	}
	// Werte aus Config, Fallback auf Defaults falls Manager nicht erreichbar
	const FString url       = FString::Printf(TEXT("http://127.0.0.1:%d/v1/completions"),
									hermesMgr ? hermesMgr->GetPort() : 8080);
	const float temperature = hermesMgr ? hermesMgr->GetTemperature() : 0.7f;
	const int32 configMax   = hermesMgr ? hermesMgr->GetMaxTokens() : 256;
	const int32 maxTokens   = FMath::Min(_request.MaxTokens, configMax);

	// ===================================================
	//						JSON-Body 
	// ===================================================
	TSharedPtr<FJsonObject> jsonBody = MakeShareable(new FJsonObject());
	jsonBody->SetStringField(TEXT("prompt"),     formattedPrompt);
	jsonBody->SetNumberField(TEXT("max_tokens"), maxTokens);
	jsonBody->SetNumberField(TEXT("temperature"), temperature);

	TArray<TSharedPtr<FJsonValue>> stopSequences;
	stopSequences.Add(MakeShareable(new FJsonValueString(TEXT("<|im_end|>"))));
	stopSequences.Add(MakeShareable(new FJsonValueString(TEXT("<|im_start|>"))));
	jsonBody->SetArrayField(TEXT("stop"), stopSequences);

	if (!_request.JsonSchema.IsEmpty())
	{
		TSharedPtr<FJsonObject> schemaObj;
		const TSharedRef<TJsonReader<>> schemaReader = TJsonReaderFactory<>::Create(_request.JsonSchema);
		if (FJsonSerializer::Deserialize(schemaReader, schemaObj) && schemaObj.IsValid())
			jsonBody->SetObjectField(TEXT("json_schema"), schemaObj);
	}

	FString jsonPayload;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&jsonPayload);
	FJsonSerializer::Serialize(jsonBody.ToSharedRef(), Writer);

	// ===================================================
	// HTTP – der Worker-Thread wartet, der Game Thread läuft weiter.
	// Geteilter Zustand (TSharedRef), damit der Callback NIE auf Stack-Variablen
	// zugreift. Plus Poll-Wait mit Abbruch → kein Deadlock beim PIE-Ende.
	// ===================================================
	TSharedRef<FHermesHttpReqState, ESPMode::ThreadSafe> state =
		MakeShared<FHermesHttpReqState, ESPMode::ThreadSafe>();
	state->Response.RequestID  = _request.RequestID;
	state->Response.ServerUsed = TEXT("Hermes");
	state->DoneEvent = FPlatformProcess::GetSynchEventFromPool(true /*manual reset*/);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> httpReq =
		FHttpModule::Get().CreateRequest();
	httpReq->SetURL(url);
	httpReq->SetVerb(TEXT("POST"));
	httpReq->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	httpReq->SetContentAsString(jsonPayload);
	httpReq->SetTimeout(30.0f);

	const double requestStart = FPlatformTime::Seconds();
	ALLMRequestManager* mgr = m_requestManager;

	httpReq->OnProcessRequestComplete().BindLambda(
		[state, mgr, _request, requestStart]
		(FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bSuccess)
		{
			// Worker hat abgebrochen (z.B. PIE-Ende) → nichts mehr anfassen.
			if (state->bAborted.load()) return;

			state->Response.ProcessTime =
				static_cast<float>(FPlatformTime::Seconds() - requestStart);

			if (bSuccess && Response.IsValid() && Response->GetResponseCode() == 200)
			{
				const FString rawJson = Response->GetContentAsString();
				if (mgr) mgr->SaveResponseToFile(rawJson, TEXT("Hermes"), _request.NPCName);
				if (mgr) state->Response.Content = mgr->ParseJsonResponse(rawJson);
				state->Response.bSuccess = true;

				// Auf den Game Thread; Singleton-Check schützt vor zerstörtem Manager.
				FLLMResponse responseCopy = state->Response;
				AsyncTask(ENamedThreads::GameThread, [mgr, responseCopy]()
				{
					if (ALLMRequestManager::Get() == mgr)
						mgr->BroadcastResponse(responseCopy);
				});
			}
			else
			{
				const int32 code = Response.IsValid() ? Response->GetResponseCode() : -1;
				if (code == 503)
				{
					UE_LOG(LogTemp, Log, TEXT("[HermesWorker] Modell laedt noch (503) – weiter an Qwen"));	
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("[HermesWorker] HTTP %d – weiter an Qwen"), code);	
				}

				// Qwen übernimmt (auf dem Game Thread, gleicher Schutz).
				FLLMRequest reqCopy = _request;
				AsyncTask(ENamedThreads::GameThread, [mgr, reqCopy]()
				{
					if (ALLMRequestManager::Get() == mgr)
						mgr->RouteToQwen(reqCopy);
				});
			}

			if (state->DoneEvent) state->DoneEvent->Trigger();
		});

	httpReq->ProcessRequest();

	// Warten – aber regelmäßig prüfen, ob der Thread gestoppt wird. S
	while (!state->DoneEvent->Wait(50))
	{
		if (m_bShouldStop.load())
		{
			state->bAborted.store(true);  // Callback ignoriert die Antwort ab jetzt
			httpReq->CancelRequest();     // HTTP abbrechen
			break;
		}
	}

	FPlatformProcess::ReturnSynchEventToPool(state->DoneEvent);
	state->DoneEvent = nullptr;
}
