#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"
#include <condition_variable>
#include <mutex>
#include <atomic>

struct FLLMRequest;
class ALLMRequestManager;

// ============================================================
//  FQwenWorker
//
//  Dedizierter Worker Thread für Qwen3.5-4B (Port 8081)
//  Priorität: TPri_BelowNormal (unter Hermes Worker)
//
//  Unterschiede zu FHermesWorker:
//    - FIFO-Queue (TQueue MPSC, lock-free) statt Priority Queue
//    - Halbe MaxTokens für schnellere Antwortzeit
//    - temperature 0.3 statt 0.7 (deterministischer)
//    - Verarbeitet: NPC-NPC-Dialog, Nacht-Planung, Fallback
//    - Kein weiterer Fallback wenn Qwen fehlschlägt
// ============================================================
class FQwenWorker : public FRunnable
{
public:
	explicit FQwenWorker(ALLMRequestManager* _requestManager);
	virtual ~FQwenWorker() = default;
	
	// ===== FRunnable Interface =====
	virtual bool   Init()  override;
	virtual uint32 Run()   override;
	virtual void   Stop()  override;
	virtual void   Exit()  override;
	
	// ===== Public API (Game-Thread-sicher) =====
	void EnqueueRequest(const FLLMRequest& _request);

private:
	void ProcessRequest(const FLLMRequest& _request);
	
	
	ALLMRequestManager* m_requestManager = nullptr;

	// TQueue MPSC: lock-free für Enqueue vom Game Thread
	TQueue<FLLMRequest, EQueueMode::Mpsc> m_queue;

	// condition_variable für Schlaf-Mechanismus
	std::condition_variable m_cv;
	std::mutex              m_cvMutex;
	std::atomic<bool>       m_bQueueHasWork { false };
	std::atomic<bool>       m_bShouldStop   { false };
};
