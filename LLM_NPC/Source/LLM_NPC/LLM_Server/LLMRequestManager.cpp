#include "LLMRequestManager.h"
#include "LLM_NPC/LLM_Server/Worker/HermesWorker.h"
#include "LLM_NPC/LLM_Server/Worker/QwenWorker.h"
#include "HAL/RunnableThread.h"
#include "Async/Async.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "LLM_NPC/LLM_Server/HermesServerManager.h"
#include "LLM_NPC/LLM_Server/QwenServerManager.h"



ALLMRequestManager* ALLMRequestManager::m_singletonInstance = nullptr;

// ===== CONSTRUCTOR =====
ALLMRequestManager::ALLMRequestManager()
{
	PrimaryActorTick.TickInterval = 0.05f; //20Hz
	PrimaryActorTick.bCanEverTick = true;
}

// ===== LIFECYCLE =====
void ALLMRequestManager::BeginPlay()
{
	Super::BeginPlay();
	
	// ===== SINGLETON VALIDATION =====
	if (m_singletonInstance != nullptr && m_singletonInstance != this)
	{
		Destroy();
		return;
	}
	
	m_singletonInstance = this;
	m_bInitialized = true;
	
	// ===== RESET COUNTERS =====
	m_hermesProcessingCount.Reset();
	m_qwenProcessingCount.Reset();
	m_bHermesOverloaded = false;
	m_pendingRequestCounter.Reset();
	
	// ===== Worker Threads Starting =====
	m_hermesWorker = new FHermesWorker(this);
	m_hermesThread = FRunnableThread::Create(
		m_hermesWorker,
		TEXT("LLM_HermesWorkerThread"),
		0,
		TPri_SlightlyBelowNormal
		);
	
	m_qwenWorker = new FQwenWorker(this);
	m_qwenThread = FRunnableThread::Create(
		m_qwenWorker,
		TEXT("LLM_QwenWorkerThread"),
		0,
		TPri_BelowNormal
		);
	
}

void ALLMRequestManager::EndPlay(const EEndPlayReason::Type _endPlayReason)
{
	// Worker Threads stoppen
	if (m_hermesWorker) m_hermesWorker->Stop();
	if (m_qwenWorker)   m_qwenWorker->Stop();

	if (m_hermesThread)
	{
		m_hermesThread->WaitForCompletion();
		delete m_hermesThread;
		m_hermesThread = nullptr;
	}
	if (m_qwenThread)
	{
		m_qwenThread->WaitForCompletion();
		delete m_qwenThread;
		m_qwenThread = nullptr;
	}

	delete m_hermesWorker; m_hermesWorker = nullptr;
	delete m_qwenWorker;   m_qwenWorker   = nullptr;

	
	if (m_singletonInstance == this)
	{
		m_singletonInstance = nullptr;
		m_bInitialized = false;
	}
	
	Super::EndPlay(_endPlayReason);
}

// ===== SINGLETON GETTER =====
ALLMRequestManager* ALLMRequestManager::Get()
{
	if (!m_singletonInstance)
	{
		return nullptr;
	}
	return m_singletonInstance;
}

// ===== MAIN TICK =====
void ALLMRequestManager::Tick(float _deltaTime)
{
	Super::Tick(_deltaTime);
}

// ===== READINESS =====
bool ALLMRequestManager::AreServersReady(const UWorld* _world)
{
	const UGameInstance* gi = _world ? _world->GetGameInstance() : nullptr;
	if (!gi) return false;

	const UHermesServerManager* h = gi->GetSubsystem<UHermesServerManager>();
	const UQwenServerManager*   q = gi->GetSubsystem<UQwenServerManager>();

	return h && q && h->IsServerRunning() && q->IsServerRunning();
}

// ===== PUBLIC API =====
void ALLMRequestManager::SubmitRequest(const FLLMRequest& _request)
{
	if (!m_bInitialized || !m_hermesWorker || !m_qwenWorker) return;

	if (_request.bPreferQwen)
	{
		RouteToQwen(_request);
		m_pendingRequestCounter.Increment();
		return;
	}

	bool bExpected = false;
	if (bHermesBusy.compare_exchange_strong(bExpected, true))
	{
		m_hermesProcessingCount.Increment();
		m_hermesWorker->EnqueueRequest(_request);
	}
	else
	{
		RouteToQwen(_request);
	}
	m_pendingRequestCounter.Increment();
}

void ALLMRequestManager::SubmitRequestWithPriority(
	const FString& _prompt, int32 _maxTokens, int32 _priority)
{
	if (!m_bInitialized || !m_hermesWorker || !m_qwenWorker) return;

	FLLMRequest request;
	request.RequestID = FGuid::NewGuid().ToString();
	request.Prompt    = _prompt;
	request.MaxTokens = _maxTokens;
	request.Timestamp = GetWorld()->GetTimeSeconds();
	request.Priority  = _priority;

	bool bExpected = false;
	if (bHermesBusy.compare_exchange_strong(bExpected, true))
	{
		m_hermesProcessingCount.Increment();
		m_hermesWorker->EnqueueRequest(request);
	}
	else
	{
		RouteToQwen(request);
	}
	m_pendingRequestCounter.Increment();
}

void ALLMRequestManager::RouteToQwen(const FLLMRequest& _request)
{
	if (!m_qwenWorker) return;

	m_qwenProcessingCount.Increment();
	m_qwenWorker->EnqueueRequest(_request);
	UE_LOG(LogTemp, Warning, TEXT("[LLM] Request -> Qwen (Fallback)"));
}

int32 ALLMRequestManager::GetQueueSize() const
{
	return m_pendingRequestCounter.GetValue();
}

// ===== BROADCAST RESPONSE =====
void ALLMRequestManager::BroadcastResponse(const FLLMResponse& _response)
{
	m_pendingRequestCounter.Decrement();

	if (_response.ServerUsed == TEXT("Hermes"))
	{
		if (m_hermesProcessingCount.GetValue() > 0)
			m_hermesProcessingCount.Decrement();
		bHermesBusy.store(false);
	}
	else
	{
		if (m_qwenProcessingCount.GetValue() > 0)
			m_qwenProcessingCount.Decrement();
		bQwenBusy.store(false);
	}

	m_bHermesOverloaded = (m_hermesProcessingCount.GetValue() > m_hermesmaxRequest);
	OnResponseReceived.Broadcast(_response, _response.bSuccess);

}

// ===== JSON LOGGING =====
void ALLMRequestManager::SaveResponseToFile(const FString& _rawJson, const FString& _prefix, const FString& _npcName)
{
	// NPC-Namen fuer Dateinamen entschaerfen (Leerzeichen etc. raus)
	FString safeName = _npcName.IsEmpty() ? TEXT("Unknown") : _npcName;
	safeName = safeName.Replace(TEXT(" "), TEXT("_"));

	FString timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	FString filename  = FString::Printf(TEXT("%s_%s_%s.json"), *_prefix, *safeName, *timestamp);
	FString outputDir = FPaths::ProjectDir() + TEXT("Saved/LLMResponses/");
	FString fullPath  = outputDir + filename;

	// Roh-JSON in einen Wrapper packen, der den NPC + Server mitfuehrt.
	// _rawJson ist bereits valides JSON und wird als verschachtelter Wert eingebettet.
	FString wrapped = FString::Printf(
		TEXT("{\n  \"npc\": \"%s\",\n  \"server\": \"%s\",\n  \"response\": %s\n}"),
		*_npcName, *_prefix, *_rawJson);

	UE_LOG(LogTemp, Warning, TEXT("[LLM] Speicherpfad: %s"), *fullPath);

	IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!platformFile.DirectoryExists(*outputDir))
	{
		platformFile.CreateDirectoryTree(*outputDir);
	}

	FFileHelper::SaveStringToFile(wrapped, *fullPath);
	UE_LOG(LogTemp, Log, TEXT("[LLM] JSON gespeichert (NPC '%s'): %s"), *_npcName, *fullPath);
}

FString ALLMRequestManager::ParseJsonResponse(const FString& _jsonString)
{
	TSharedPtr<FJsonObject> jsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(_jsonString);

	if (!FJsonSerializer::Deserialize(Reader, jsonObject) || !jsonObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[LLM] ParseJsonResponse fehlgeschlagen: %s"), *_jsonString);
		return TEXT("");
	}

	// Kein choices-Array → direkt text/content versuchen
	if (!jsonObject->HasField(TEXT("choices")))
	{
		FString fallback;
		if (jsonObject->TryGetStringField(TEXT("text"),    fallback)) return fallback;
		if (jsonObject->TryGetStringField(TEXT("content"), fallback)) return fallback;
		return TEXT("");
	}

	TArray<TSharedPtr<FJsonValue>> Choices = jsonObject->GetArrayField(TEXT("choices"));
	if (Choices.Num() == 0) return TEXT("");

	TSharedPtr<FJsonObject> Choice = Choices[0]->AsObject();
	if (!Choice.IsValid()) return TEXT("");

	// Hermes → choices[0].text
	FString text;
	if (Choice->TryGetStringField(TEXT("text"), text)) return text;

	// Qwen → choices[0].message.content
	const TSharedPtr<FJsonObject>* MessageObj;
	if (!Choice->TryGetObjectField(TEXT("message"), MessageObj)) return TEXT("");

	FString content;
	if ((*MessageObj)->TryGetStringField(TEXT("content"), content)) return content;

	return TEXT("");
}

