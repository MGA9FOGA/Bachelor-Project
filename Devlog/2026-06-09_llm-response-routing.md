# Devlog – 2026-06-09

## LLM-Antwort-Routing per RequestID + Repo-Aufräumen

**Branch:** fix/llm-response-routing **Commits:** 6e1cc90 (Code-Fixes), 2291110 (Build-Artefakte / .gitignore) **Status:** ⚠️ noch nicht im Editor kompiliert/getestet


## TL;DR

Drei Sachen behoben:

1. **Kritisch:** LLM-Antworten wurden global an *alle* NPCs verteilt, ohne nach RequestID zu filtern → mit \>1 NPC chaotisch. Jetzt zielgerichtetes Routing.

2. **Bug:** GetQueueSize() zählte doppelt (doppelter Increment()).

3. **Repo:** ~1860 eingecheckte Build-Artefakte entfernt + .gitignore repariert.


## 1. Das eigentliche Problem (RequestID-Routing)

### Symptom

ALLMRequestManager::OnResponseReceived ist ein **globaler** DYNAMIC\_MULTICAST-Delegate. Daran hingen gleichzeitig:

- jeder einzelne ANPCCharacter (via OnResponseReceived.AddDynamic in BeginPlay)

- das UChatWidget

Niemand prüfte die RequestID der Antwort. Folge bei mehr als einem NPC:

- **NPC B** verarbeitete die Antwort, die für **NPC A** bestimmt war → falsche Emotion/Memory in NPC B.

- Das Chat-Fenster konnte eine Antwort anzeigen, die zu einem anderen NPC gehört.

Die RequestID war zwar vorhanden und wurde sauber durch die Worker durchgereicht – nur **nie ausgewertet**.

### Lösung – Idee

Das ChatWidget besitzt die Konversation. Es weiß:

- welche RequestID es zuletzt abgeschickt hat, und

- mit welchem NPC es gerade spricht.

Damit:

- filtert das Widget Antworten nach RequestID (alles Fremde wird verworfen), und

- leitet das geparste Ergebnis **direkt** an genau diesen einen NPC weiter.

Das globale Pro-NPC-Abo entfällt komplett.

### Geänderte Dateien

#### UI/ChatWidget.h

```
+class ANPCCharacter;  
 ...  
-	void OpenForNPC(const FString& \_npcSystemPrompt);  
+	void OpenForNPC(const FString& \_npcSystemPrompt, ANPCCharacter\* \_npc = nullptr);  
 ...  
    FString m\_pendingRequestID;  
+  
+	// NPC, mit dem das Widget aktuell spricht (Antwort-Routing)  
+	TWeakObjectPtr\<ANPCCharacter\> m\_activeNPC;
```

- Default = nullptr beim neuen Parameter, damit bestehende Aufrufe (auch evtl. Blueprint) weiter kompilieren.

- TWeakObjectPtr, damit das Widget keinen NPC am Leben hält, der zerstört wird.

#### UI/ChatWidget.cpp

```
 \#include "LLM\_NPC/LLM\_Server/LLMRequestManager.h"  
+\#include "LLM\_NPC/NPC/NPCCharacter.h"
```

**SendMessage()** – gesendete ID merken:

```
    m\_conversationHistory.Add(\{TEXT("user"), userText\});  
+  
+	// Nur Antworten mit genau dieser ID akzeptieren  
+	m\_pendingRequestID = request.RequestID;  
    manager-\>SubmitRequest(request);
```

**OpenForNPC()** – aktiven NPC merken, alten Pending-State zurücksetzen:

```
-void UChatWidget::OpenForNPC(const FString& \_npcSystemPrompt)  
+void UChatWidget::OpenForNPC(const FString& \_npcSystemPrompt, ANPCCharacter\* \_npc)  
 \{  
    m\_systemPrompt = \_npcSystemPrompt;  
+  
+	m\_activeNPC = \_npc;  
+	m\_pendingRequestID.Empty();
```

**OnLLMResponse()** – nach ID filtern (oben) und Ergebnis an den NPC routen (unten):

```
 void UChatWidget::OnLLMResponse(const FLLMResponse& \_response, bool \_bSuccess)  
 \{  
+	// Gehört diese Antwort zu unserem zuletzt gesendeten Request?  
+	if (!m\_pendingRequestID.IsEmpty() && \_response.RequestID != m\_pendingRequestID)  
+		return;  
+	m\_pendingRequestID.Empty();  
+  
    if (m\_messageList && ...)

    while (m\_conversationHistory.Num() \> 20)  
        m\_conversationHistory.RemoveAt(0);  
+  
+	// Ergebnis gezielt an den NPC weitergeben (Emotion / Memory / Blackboard)  
+	if (ANPCCharacter\* npc = m\_activeNPC.Get())  
+		npc-\>ApplyDialogueResult(reply, m\_lastIntent, m\_lastMood);  
 \}
```

#### NPC/NPCCharacter.cpp

Globales Abo raus:

```
    InitializeECS();  
-	LoadTestSchedule();   
-  
-	if (ALLMRequestManager\* manager = ALLMRequestManager::Get())  
-	\{  
-		manager-\>OnResponseReceived.AddDynamic(  
-			this, &ANPCCharacter::OnLLMResponseReceived);  
-	\}  
+	LoadTestSchedule();  
+  
+	// Bewusst KEIN globales OnResponseReceived-Abo mehr – siehe ApplyDialogueResult()  
 \}
```

OnLLMResponseReceived(FLLMResponse, bool) → ersetzt durch ApplyDialogueResult(reply, intent, mood). Statt die Antwort selbst zu parsen, bekommt die Methode die bereits geparsten Werte vom Widget:

```
-void ANPCCharacter::OnLLMResponseReceived(const FLLMResponse& Response, bool bSuccess)  
+void ANPCCharacter::ApplyDialogueResult(const FString& \_reply, EIntentType \_intent, EEmotionType \_mood)  
 \{  
-	if (!bSuccess || Response.Content.IsEmpty()) return;  
-	FNPCDialogResponse parsed = ParseLLMResponse(Response.Content);  
-  
    if (FEmotionData\* emotion = GetEmotion())  
-		emotion-\>TriggerEmotion(parsed.Mood, 0.7f, GetWorld()-\>GetTimeSeconds());  
+		emotion-\>TriggerEmotion(\_mood, 0.7f, GetWorld()-\>GetTimeSeconds());  
    ...
```

(Rest analog: parsed.Reply → \_reply, parsed.Intent → \_intent.)

#### NPC/NPCCharacter.h

```
-	UFUNCTION()  
-	void OnLLMResponseReceived(const FLLMResponse& Response, bool bSuccess);  
+	// Wird vom ChatWidget gezielt für genau diesen NPC aufgerufen  
+	void ApplyDialogueResult(const FString& \_reply, EIntentType \_intent, EEmotionType \_mood);
```

#### LLM\_NPCCharacter.cpp (Player)

Interact() übergibt den NPC ans Widget:

```
    FString prompt = interactable-\>GetInteractionPrompt();  
    if (!prompt.IsEmpty())  
    \{  
-		m\_chatWidgetInstance-\>OpenForNPC(prompt);  
+		ANPCCharacter\* npc = Cast\<ANPCCharacter\>(m\_nearbyInteractable);  
+		m\_chatWidgetInstance-\>OpenForNPC(prompt, npc);  
    \}
```

### Neuer Datenfluss

```
Player drückt Interact  
  └─ NPC-\>GetInteractionPrompt()  (System-Prompt)  
  └─ ChatWidget-\>OpenForNPC(prompt, NPC)        // merkt sich NPC  
        │  
Spieler tippt -\> SendMessage()  
  └─ m\_pendingRequestID = request.RequestID     // merkt sich ID  
  └─ Manager-\>SubmitRequest(request)  
        │  
Worker (Hermes/Qwen) -\> Antwort mit gleicher RequestID  
        │  
ChatWidget::OnLLMResponse()  
  └─ if RequestID != m\_pendingRequestID -\> return   // Fremde ignorieren  
  └─ Anzeige (Typewriter)  
  └─ m\_activeNPC-\>ApplyDialogueResult(reply, intent, mood)  // nur DIESER NPC
```


## 2. Doppelter Increment in SubmitRequest

LLM\_Server/LLMRequestManager.cpp:

```
    m\_pendingRequestCounter.Increment();  
-  
-	m\_pendingRequestCounter.Increment();  
 \}
```

GetQueueSize() lieferte vorher den doppelten Wert – relevant z. B. für IsHermesOverloaded()-Logik und Debug-Anzeigen.


## 3. Build-Artefakte aus dem Repo (Commit 2291110)

- Binaries/, Intermediate/, Saved/ sind generierter Build-Output. **~1860 Dateien** (DLLs, PDBs, .target etc.) lagen im Repo.

- Per git rm --cached aus dem Index entfernt → **bleiben lokal auf der Platte**, werden beim nächsten Build neu erzeugt.

- .gitignore neu, deckt jetzt ab:

- ```
Binaries/  Intermediate/  Saved/  DerivedDataCache/  
.vs/  .idea/  \*.VC.db  
\*.exe  \*.pdb  \*.pch  
LLM\_NPC/Deploy/Models/\*.gguf
```

> Der riesige Lösch-Diff stammt **nur** aus diesem Commit und lässt sich isoliert reverten, falls Binaries bewusst im Repo bleiben sollen.


## Risiken / offen

- **Nicht kompiliert** (keine UE-Toolchain in der Umgebung). Vor dem Merge im Editor bauen.

- Signaturänderung OpenForNPC(...): falls ein Blueprint die Funktion aufruft, muss der Node ggf. neu verbunden werden (Default-Param mildert das ab).

- ParseLLMResponse() im NPC ist jetzt ungenutzt (Parsing passiert im Widget) – bewusst belassen, kann später raus.

## Bewusst NICHT in diesem PR (Backlog)

- Hardcodierte D:/LLM-NPC/...-Pfade im HermesServerManager → FPaths::ProjectDir()

- Nicht verdrahtete LLM-Planung: GenerateSchedule/Memory/Needs/EmotionPrompt werden nie abgeschickt

- Toter Code im ChatWidget: BuildPrompt, ExtractNPCResponse

- bQwenBusy wird nie auf true gesetzt

- Format-Tippfehler %.0f% in GenerateHermesPrompt

