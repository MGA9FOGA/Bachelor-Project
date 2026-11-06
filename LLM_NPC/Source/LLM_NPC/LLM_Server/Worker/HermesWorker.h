#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "LLM_NPC/LLM_Server/LLMTypes.h"
#include "LLM_NPC/LLM_Server/LLMRequestManager.h" 
#include <condition_variable>
#include <mutex>
#include <queue>
#include <atomic>


class ALLMRequestManager;

// ============================================================
//  FHermesWorker – dedizierter CPU-Thread für Hermes 3
//
//  WAS DIESER THREAD MACHT (alles CPU-seitig):
//    1. Schlafen via condition_variable (kein Spin-Wait)
//    2. FLLMRequest aus der Priority-Queue nehmen
//    3. JSON-Body serialisieren  ← CPU
//    4. HTTP-Request an localhost:8080 senden
//    5. Auf HTTP-Antwort warten  ← Worker-Thread blockiert, nicht Game Thread
//    6. JSON-Response parsen     ← CPU
//    7. Callback via AsyncTask auf den Game Thread dispatchen
//
//  WAS NICHT IN DIESEM THREAD PASSIERT:
//    - Die tatsächliche LLM-Inferenz (läuft auf GPU via Vulkan in llama.cpp)
//    - ECS-Schreibzugriffe (immer nur via Callback auf Game Thread)
//    - Render-Operationen
// ============================================================

class FHermesWorker : public FRunnable
{
public:
	explicit FHermesWorker(ALLMRequestManager* _requestManager);
	virtual ~FHermesWorker() = default;
	
	// ===== FRunnable Interface =====
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;
	
	// ===== Öffentliche API (Game-Thread-sicher) =====
	
	// Fügt eine Anfrage in die Prio-Queue ein und weckt den Worker
	void EnqueueRequest(const FLLMRequest& _request);
private:
	// Führt den eigentlichen HTTP-Call durch – blockiert den Worker Thread
	// bis llama.cpp antwortet. Der Game Thread läuft dabei weiter.
	void ProcessRequest(const FLLMRequest& _request);
	
	ALLMRequestManager* m_requestManager = nullptr;
	
	//Priority Queue + Guard
	std::priority_queue<FLLMRequest> m_queue;
	mutable FCriticalSection m_queueMutex;
	
	// Condition Variable zum Schlafen (kein-Busy-Wait)
	std::condition_variable m_cv;
	std::mutex m_cvMutex;
	
	std::atomic<bool> m_bShouldStop {false};
};
