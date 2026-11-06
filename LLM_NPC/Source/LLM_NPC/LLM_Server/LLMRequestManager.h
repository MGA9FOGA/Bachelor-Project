#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Containers/Queue.h"
#include <atomic>

#include "LLMTypes.h"
#include "LLMRequestManager.generated.h"


// ===== Forward Declarations =====
class FHermesWorker;
class FQwenWorker;
class FRunnableThread;
class IHttpRequest;
class IHttpResponse;

using FHttpRequestPtr  = TSharedPtr<IHttpRequest,  ESPMode::ThreadSafe>;
using FHttpResponsePtr = TSharedPtr<IHttpResponse, ESPMode::ThreadSafe>;

// ===== ENUMS =====
UENUM(BlueprintType)
enum class ELLMStatus : uint8
{
	Pending =	0 UMETA(DisplayName = "Pending"),
	Running =	1 UMETA(DisplayName = "Running"),
	Done =		2 UMETA(DisplayName = "Done"),
	Failed =	3 UMETA(DisplayName = "Failed")
};

// ===== REQUEST STRUCT =====
// ============================================================
//  Zentraler Request-Container
//  Wird vom Game Thread erstellt, vom Worker Thread konsumiert
// ============================================================
USTRUCT(BlueprintType)
struct FLLMRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	FString RequestID;

	// Name des NPCs, der diese Anfrage ausgelöst hat (fürs Logging/JSON)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	FString NPCName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	FString Prompt;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	FString SystemPrompt;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	TArray<FString> HistoryRoles;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	TArray<FString> HistoryContents;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	FString UserMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	int32 MaxTokens = 256;

	// Optionales JSON-Schema (als String). Ist es gesetzt, zwingt llama.cpp die
	// Ausgabe grammatikalisch in genau diese Form → garantiert valides JSON.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	FString JsonSchema;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	float Timestamp = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	ELLMStatus Status = ELLMStatus::Pending;
	
	// Priorität für die Worker-Queue (höher = früher verarbeitet)
	int32 Priority = static_cast<int32>(ELLMPriority::Normal);

	// Erzwingt die Verarbeitung durch den Qwen-Server statt der Hermes-first-Logik
	// (z.B. für proaktive Begrüßungen, die Hermes für den Spieler-Dialog freihalten).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	bool bPreferQwen = false;
	
	//Für std::priority_queue
	bool operator<(const FLLMRequest& Other) const 
	{
		if (Priority != Other.Priority)
			return Priority < Other.Priority;  

		return Timestamp > Other.Timestamp;
	};
};

// ===== RESPONSE STRUCT =====
USTRUCT(blueprintType)
struct FLLMResponse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	FString RequestID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	FString Content;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	FString ServerUsed; //Hermes oder Qwen

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	float ProcessTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LLM")
	bool bSuccess = false;
};

// ===== DELEGATE =====
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnLLMResponse,
	const FLLMResponse&, Response,
	bool, bSuccess
);


// ===== MANAGER ACTOR =====
UCLASS()
class LLM_NPC_API ALLMRequestManager : public AActor
{
	GENERATED_BODY()

public:
	ALLMRequestManager();
	
	virtual void BeginPlay() override;
	virtual void Tick(float _deltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type _endPlayReason) override;

	// ===== PUBLIC API =====
	UFUNCTION(BlueprintCallable, Category = "LLM")
	void SubmitRequest(const FLLMRequest& _request);
	
	// ===== Overlad mit Priority =====
	void SubmitRequestWithPriority(const FString& _prompt, int32 _maxTokens = 256, int32 _priority = static_cast<int32>(ELLMPriority::Low));

	UFUNCTION(BlueprintPure, Category = "LLM")
	int32 GetQueueSize() const;

	// ===== SINGLETON =====
	UFUNCTION(BlueprintPure, Category = "LLM")
	static ALLMRequestManager* Get();

	// Sind BEIDE LLM-Server (Hermes + Qwen) online/bereit?
	static bool AreServersReady(const UWorld* _world);

	UFUNCTION(BlueprintPure, Category = "LLM")
	bool AreBothServersReady() const { return AreServersReady(GetWorld()); }
	
	// ===== DELEGATES =====
	UPROPERTY(BlueprintAssignable, Category = "LLM")
	FOnLLMResponse OnResponseReceived;
	
	// ===== STATUS INFO =====
	UFUNCTION(BlueprintPure, Category = "LLM")
	bool IsHermesOverloaded() const { return m_bHermesOverloaded; }

	UFUNCTION(BlueprintPure, Category = "LLM")
	int32 GetHermesProcessingCount() const { return m_hermesProcessingCount.GetValue(); }

	UFUNCTION(BlueprintPure, Category = "LLM")
	int32 GetQwenProcessingCount() const { return m_qwenProcessingCount.GetValue(); }

	std::atomic<bool> bHermesBusy {false};
	std::atomic<bool> bQwenBusy {false};
	
	// Wird von FHermesWorker aufgerufen wenn Hermes fehlschlägt
	void RouteToQwen(const FLLMRequest& _request);
	
	// Wird von Worker-Threads aufgerufen um Response zurückzuspielen
	// Läuft dann über AsyncTask auf dem Game Thread
	void BroadcastResponse(const FLLMResponse& _response);
	
	// ===== HELPER METHODS =====
	FString ParseJsonResponse(const FString& _jsonString);
	void SaveResponseToFile(const FString& _rawJson, const FString& _prefix, const FString& _npcName = TEXT(""));

private:
	// ===== SINGLETON INSTANCE =====
	static ALLMRequestManager* m_singletonInstance;
	
	// ===== Worker Threads =====
	FHermesWorker*	 m_hermesWorker	= nullptr;
	FQwenWorker*	 m_qwenWorker	= nullptr;
	FRunnableThread* m_hermesThread	= nullptr;
	FRunnableThread* m_qwenThread	= nullptr;

	// ===== QUEUE & PROCESSING =====
	FThreadSafeCounter m_pendingRequestCounter;
	FThreadSafeCounter m_hermesProcessingCount = 0;
	FThreadSafeCounter m_qwenProcessingCount = 0;
	FThreadSafeBool m_bHermesOverloaded = false;
	
	// ===== STATUS INFO =====
	int32 m_hermesmaxRequest = 5;
	int32 m_qwenmaxRequest = 10;
	bool m_bInitialized = false; 
	
	
};
