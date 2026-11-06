#include "QwenWorker.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "LLM_NPC/LLM_Server/LLMRequestManager.h"
#include "Async/Async.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "LLM_NPC/LLM_Server/QwenServerManager.h"

namespace
{
	struct FQwenHttpReqState
	{
		FLLMResponse      Response;
		std::atomic<bool> bAborted{ false };
		FEvent*           DoneEvent = nullptr;
	};
}

FQwenWorker::FQwenWorker(ALLMRequestManager* _requestManager)
	: m_requestManager(_requestManager)
{
}

bool FQwenWorker::Init()
{
	UE_LOG(LogTemp, Log, TEXT("[QwenWorker] Thread initialisiert (TPri_BelowNormal)"));
	return true;
}

uint32 FQwenWorker::Run()
{
	while (!m_bShouldStop.load())
	{
		{
			std::unique_lock<std::mutex> lock(m_cvMutex);
			m_cv.wait(lock, [this]()
			{
				return m_bQueueHasWork.load() || m_bShouldStop.load();
			});

			if (m_bShouldStop.load()) break;
		}

		FLLMRequest request;
		while (m_queue.Dequeue(request))
		{
			if (m_bShouldStop.load()) break;
			ProcessRequest(request);
		}

		m_bQueueHasWork.store(false);
	}

	return 0;
}

void FQwenWorker::Stop()
{
	m_bShouldStop.store(true);
	m_cv.notify_all();
}

void FQwenWorker::Exit()
{
	UE_LOG(LogTemp, Log, TEXT("[QwenWorker] Thread beendet"));
}

// ===== QUEUE API =====
void FQwenWorker::EnqueueRequest(const FLLMRequest& _request)
{
	m_queue.Enqueue(_request); // lock-free MPSC
	m_bQueueHasWork.store(true);
	m_cv.notify_one();
}

// ===== HTTP CALL =====
void FQwenWorker::ProcessRequest(const FLLMRequest& _request)
{
	UQwenServerManager* qwenMgr = nullptr;
	if (m_requestManager)
	{
		if (UGameInstance* gi = m_requestManager->GetGameInstance())
		{
			qwenMgr = gi->GetSubsystem<UQwenServerManager>();
		}
	}

	const FString url = FString::Printf(TEXT("http://127.0.0.1:%d/v1/chat/completions"),
	                                    qwenMgr ? qwenMgr->GetPort() : 8081);
	const float temperature = qwenMgr ? qwenMgr->GetTemperature() : 0.3f;
	const int32 configMax = qwenMgr ? qwenMgr->GetMaxTokens() : 128;
	const int32 maxTokens = FMath::Min(_request.MaxTokens / 2, configMax);

	// ===== MESSAGES ARRAY =====
	TArray<TSharedPtr<FJsonValue>> messages;

	// System Message mit /no_think
	TSharedPtr<FJsonObject> systemMsg = MakeShareable(new FJsonObject());
	systemMsg->SetStringField(TEXT("role"), TEXT("system"));
	systemMsg->SetStringField(TEXT("content"), FString::Printf(TEXT("/no_think %s"), *_request.SystemPrompt));
	messages.Add(MakeShareable(new FJsonValueObject(systemMsg)));

	// Verlauf
	for (int32 i = 0; i < _request.HistoryRoles.Num(); i++)
	{
		TSharedPtr<FJsonObject> histMsg = MakeShareable(new FJsonObject());
		histMsg->SetStringField(TEXT("role"), *_request.HistoryRoles[i]);
		histMsg->SetStringField(TEXT("content"), *_request.HistoryContents[i]);
		messages.Add(MakeShareable(new FJsonValueObject(histMsg)));
	}

	// Aktuelle User Message
	TSharedPtr<FJsonObject> userMsg = MakeShareable(new FJsonObject());
	userMsg->SetStringField(TEXT("role"), TEXT("user"));
	userMsg->SetStringField(TEXT("content"), *_request.UserMessage);
	messages.Add(MakeShareable(new FJsonValueObject(userMsg)));

	// ===== JSON BODY =====
	TSharedPtr<FJsonObject> jsonBody = MakeShareable(new FJsonObject());
	jsonBody->SetArrayField(TEXT("messages"), messages);
	jsonBody->SetNumberField(TEXT("max_tokens"), maxTokens);
	jsonBody->SetNumberField(TEXT("temperature"), temperature);
	jsonBody->SetStringField(TEXT("model"), TEXT("local"));
	jsonBody->SetBoolField(TEXT("stream"), false);

	TSharedPtr<FJsonObject> templateKwargs = MakeShareable(new FJsonObject());
	templateKwargs->SetBoolField(TEXT("enable_thinking"), false);
	jsonBody->SetObjectField(TEXT("chat_template_kwargs"), templateKwargs);

	TArray<TSharedPtr<FJsonValue>> stopSequences;
	stopSequences.Add(MakeShareable(new FJsonValueString(TEXT("<|im_end|>"))));
	stopSequences.Add(MakeShareable(new FJsonValueString(TEXT("<|im_start|>"))));
	jsonBody->SetArrayField(TEXT("stop"), stopSequences);

	// Grammar-constrained Output: erzwingt valides JSON in der gewünschten Form.
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

	// ===== HTTP CALL =====
	// Geteilter Zustand + Poll-Wait mit Abbruch → kein Deadlock beim PIE-Ende.
	TSharedRef<FQwenHttpReqState, ESPMode::ThreadSafe> state =
		MakeShared<FQwenHttpReqState, ESPMode::ThreadSafe>();
	state->Response.RequestID  = _request.RequestID;
	state->Response.ServerUsed = TEXT("Qwen");
	state->DoneEvent = FPlatformProcess::GetSynchEventFromPool(true /*manual reset*/);

	const double requestStart = FPlatformTime::Seconds();
	const FString npcName = _request.NPCName;
	ALLMRequestManager* mgr = m_requestManager;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> httpReq =
		FHttpModule::Get().CreateRequest();
	httpReq->SetURL(url);
	httpReq->SetVerb(TEXT("POST"));
	httpReq->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	httpReq->SetContentAsString(jsonPayload);
	httpReq->SetTimeout(20.0f);

	httpReq->OnProcessRequestComplete().BindLambda(
		[state, mgr, npcName, requestStart]
		(FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bSuccess)
		{
			// Worker hat abgebrochen (z.B. PIE-Ende) → nichts mehr anfassen.
			if (state->bAborted.load()) return;

			state->Response.ProcessTime =
				static_cast<float>(FPlatformTime::Seconds() - requestStart);

			if (bSuccess && Response.IsValid() && Response->GetResponseCode() == 200)
			{
				const FString RawJson = Response->GetContentAsString();

				if (mgr) mgr->SaveResponseToFile(RawJson, TEXT("Qwen"), npcName);
				if (mgr) state->Response.Content = mgr->ParseJsonResponse(RawJson);

				state->Response.Content = state->Response.Content.TrimStartAndEnd();
				if (state->Response.Content.StartsWith(TEXT("{")))
					state->Response.Content = state->Response.Content.Mid(1);

				state->Response.bSuccess = !state->Response.Content.IsEmpty();

				UE_LOG(LogTemp, Warning, TEXT("[QwenWorker] Response: %.2fs | %s"),
					   state->Response.ProcessTime, *state->Response.Content);
			}
			else
			{
				state->Response.bSuccess = false;
				const int32 code = Response.IsValid() ? Response->GetResponseCode() : -1;

				if (code == 503)
				{
					// Normal direkt nach dem Start: Modell wird noch geladen.
					state->Response.Content = TEXT("Error: model loading");
					UE_LOG(LogTemp, Log, TEXT("[QwenWorker] Modell laedt noch (503)"));
				}
				else
				{
					state->Response.Content = TEXT("Error: Qwen request failed");
					const FString rawError = Response.IsValid() ? Response->GetContentAsString() : TEXT("no response");
					UE_LOG(LogTemp, Error, TEXT("[QwenWorker] HTTP %d – %s"), code, *rawError);
				}
			}

			// Auf den Game Thread; Singleton-Check schützt vor zerstörtem Manager.
			FLLMResponse responseCopy = state->Response;
			AsyncTask(ENamedThreads::GameThread, [mgr, responseCopy]()
			{
				if (ALLMRequestManager::Get() == mgr)
					mgr->BroadcastResponse(responseCopy);
			});

			if (state->DoneEvent) state->DoneEvent->Trigger();
		});

	httpReq->ProcessRequest();

	// Poll-Wait: nicht endlos blockieren, sondern auf Stop reagieren.
	while (!state->DoneEvent->Wait(50))
	{
		if (m_bShouldStop.load())
		{
			state->bAborted.store(true);
			httpReq->CancelRequest();
			break;
		}
	}

	FPlatformProcess::ReturnSynchEventToPool(state->DoneEvent);
	state->DoneEvent = nullptr;
}
