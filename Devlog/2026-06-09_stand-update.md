# Devlog – Stand-Update (2026-06-09)

Was seit dem dokumentierten Stand (Einträge \#1–\#5 + „Aktueller Stand") dazugekommen ist. Vom LLM-Chat-Prototyp zur NPC-Simulation.


# 6-Eintrag: Worker-Thread-Architektur (ersetzt Tick-Processing)

##### Ziel

HTTP-Calls vom Game-Thread runterholen, damit mehrere NPCs parallel Anfragen stellen können ohne den Frame zu blockieren.

##### Umgesetzt

- FHermesWorker und FQwenWorker als echte FRunnable-Worker-Threads (FRunnableThread::Create). HTTP laeuft off-game-thread; der Worker blockiert per FEvent::Wait, der Game-Thread laeuft frei weiter.

- Hermes-Worker: std::priority\_queue + std::condition\_variable + std::mutex → echte Prioritaets-Queue (FLLMRequest::operator\< nach Priority/Timestamp).

- Qwen-Worker: lock-free TQueue (MPSC) + condition\_variable.

- std::atomic\<bool\> bHermesBusy / bQwenBusy statt m\_bProcessingRequest.

- Antworten gehen via AsyncTask(ENamedThreads::GameThread, …) zurueck.

- Alte SendToHermes/OnHermesResponse/… im Manager auskommentiert.

##### Loest ToDo

„Multi-NPC: m\_bProcessingRequest zu FThreadSafeBool" — sogar weiter gegangen (volle Worker-Threads + std::atomic statt nur FThreadSafeBool).


# 7-Eintrag: Request/Response-Datenmodell erweitert

##### Umgesetzt

- FLLMRequest ist jetzt ein USTRUCT: RequestID, SystemPrompt, HistoryRoles\[\], HistoryContents\[\], UserMessage, MaxTokens, Timestamp, Priority, ELLMStatus.

- Neu: FLLMResponse (RequestID, Content, ServerUsed, ProcessTime, bSuccess).

- Neue Enums: ELLMRoute, ELLMPriority (Low/Normal/High/Urgent), ELLMStatus.


# 8-Eintrag: Hermes→Qwen-Fallback bei Fehler

##### Ziel

Bei Hermes-HTTP-Fehler nicht einfach scheitern, sondern auf Qwen ausweichen.

##### Umgesetzt

FHermesWorker leitet bei HTTP-Fehler / Statuscode ≠ 200 automatisch per RouteToQwen() an Qwen weiter. Qwen sendet dann die finale Response.

##### Loest ToDo

„Hermes-Fallback auf Qwen bei Timeout (aktuell nur bei Ueberlastung, nicht bei Fehler)."


# 9-Eintrag: Prompt-Format ChatML + strukturiertes JSON

##### Umgesetzt

- Weg von "Spieler:/NPC:" → ChatML (\<|im\_start|\>system/user/assistant).

- Assistant wird mit \{ geprimt → erzwingt JSON-Antwort.

- Qwen: /no\_think, chat\_template\_kwargs.enable\_thinking=false, /v1/chat/completions (messages-Array).

- NPC-Antworten als strukturiertes JSON: \{reply, intent, mood\} → EIntentType (Greeting/Trade/Hostile/Quest), EEmotionType (Happy/Suspicious/Angry/…).


# 10-Eintrag: Qwen-ServerManager – Sleep → Health-Polling

##### Vorher

Synchrones Sleep(5s)+Sleep(2s) als Startbestaetigung.

##### Jetzt

Asynchrones PollServerReady() wie bei Hermes (HTTP /health, 1s-Intervall, 15 Versuche) + EnableServer()/DisableServer().


# 11-Eintrag: Eigenstaendiges ECS-System

##### Ziel

Den NPC datengetrieben und unabhaengig vom UE-Component-System modellieren (serialisierbar, isoliert testbar).

##### Umgesetzt

- FNPCECS mit Entity-IDs; reine Daten-Structs als Komponenten.

- 13 Komponenten: Identity, Memory (mit Pruning + Retrieval-Score), Reflection, Emotion (Trigger + Decay), Relationship (Trust/Liking/SocialUrge), Schedule, Dialog, Conversation, Transform, NavMesh, BTState, Needs (Hunger/Exhaustion/Safety mit Decay-Raten + Schwellen), Waypoint.

- Systeme ueber Bitmask (ESystemType): MemoryDecay, EmotionDecay, Needs, Wander, TransformSync, Schedule — pro NPC aktivierbar.

- GetComponent\<T\>() per Template + if constexpr-Dispatch.

##### Loest ToDo

„ECS-Integration: Needs, SocialGraph, BehaviourTree an LLM-Kontext anbinden."


# 12-Eintrag: NPCDataAsset (datengetriebene NPCs)

##### Umgesetzt

UNPCDataAsset (PrimaryDataAsset): Name, Occupation, Personality, Goals, Home/WorkTag, Base-Needs, ActiveSystems-Bitmask (im Editor ankreuzbar), FSchedulePreferences (WakeUp/Sleep-Stunde, Routine-Freitext fuers LLM, typische Aktivitaeten).

##### Loest ToDo

„NPC-spezifische Systemprompts (Persoenlichkeit, Rolle, Weltkontext)."


# 13-Eintrag: Reicher LLM-Kontext aus dem ECS

##### Umgesetzt

- GenerateHermesPrompt() baut den System-Prompt aus Identitaet, Persoenlichkeit, Zielen, Top-5-Memories, dominanter Emotion, Needs und Beziehungen + RULES + JSON-Schema (reply/intent/mood).

- GenerateSchedule/Memory/Needs/EmotionPrompt vorbereitet (noch nicht abgeschickt).


# 14-Eintrag: Behavior Tree + AIController

##### Umgesetzt

- ANPCAIController (OnPossess → Blackboard-Init → RunBehaviorTree).

- 4 BT-Tasks: GoToLocation, Idle, CheckNeeds, PerformAction

  - CustomBTDecorator + FNPCBlackboardKeys + FNPCBT-Wrapper.

- NPC synct Goal/IsSleeping/PlayerTalking ins Blackboard (SyncGoalToBlackboard).


# 15-Eintrag: Welt-Systeme

##### Umgesetzt

- UWorldClock (TickableWorldSubsystem): Spielzeit, GetHour/Minute, RealSecondsPerGameHour (1 Tag = 12 reale Min) → speist FScheduleSystem::CurrentGameHour.

- WorldLocationManager/Loader/DataAsset, EWorldLocationTag, ENPCOccupation.

- Workstation-Reservierung (ReserveWorkstation/ReleaseWorkstation), Zielwahl nach Desire+Occupation (ChooseNextDestination), Home/Work-Navigation, Tagesablauf (Stunde→Aktivitaet→Desire).


# 16-Eintrag: Spieler-Interaktion & UI-Politur

##### Umgesetzt

- IInteractableInterface (OnInteract/GetInteractionPrompt/CanInteract), Interaktions-Sphere am NPC, Proximity-Erkennung → Chat oeffnet mit NPC-spezifischem Prompt (statt nur ToggleChat).

- Typewriter-Effekt im ChatWidget, JSON-Parsing im UI.

- SaveResponseToFile(): jede Roh-Antwort als JSON nach Saved/LLMResponses/.


# 17-Eintrag: Bugfixes (Session 2026-06-09)

Siehe separaten Devlog 2026-06-09\_llm-response-routing.md:

- RequestID-Routing (Antworten landeten global bei allen NPCs)

- Doppelter Increment() in SubmitRequest (GetQueueSize zaehlte doppelt)

- .gitignore + ~1860 eingecheckte Build-Artefakte entfernt


# Aktueller Stand & Naechste Schritte

##### Funktioniert (neu)

- Echte Worker-Threads, HTTP off-game-thread, Prio-Queue

- Hermes→Qwen-Fallback auch bei Fehlern

- Strukturierte JSON-Dialoge (reply/intent/mood) ins ECS zurueckgespielt

- Vollstaendiges ECS (Needs/Emotion/Memory/Relationship/Schedule)

- Datengetriebene NPCs ueber DataAsset

- Behavior Tree + Tagesablauf + Weltuhr + Location-Navigation

- Naehe-basierte Spieler-Interaktion mit NPC-spezifischem Prompt

##### Offen / ToDo (weiterhin)

- Streaming-Antworten (Qwen sendet aktuell explizit stream:false)

- Hardcodierte Pfade D:/LLM-NPC/... in beiden ServerManagern → FPaths::ProjectDir()

- GenerateSchedule/Memory/Needs/EmotionPrompt sind gebaut, aber nirgends abgeschickt (LLM-gesteuerte Tagesplanung noch nicht verdrahtet — laeuft auf LoadTestSchedule())

- Toter Code im ChatWidget: BuildPrompt, ExtractNPCResponse

- bQwenBusy wird nie auf true gesetzt

- Format-Tippfehler %.0f% in GenerateHermesPrompt

