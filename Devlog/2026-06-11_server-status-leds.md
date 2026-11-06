# Devlog – Server-Status-LEDs (2026-06-11)

# 19-Eintrag: UI-Statusanzeige fuer Hermes & Qwen

##### Ziel

Auf einen Blick sehen, ob die beiden llama-server laufen. Zwei Kreise: gruen = online, rot = offline.

##### Umgesetzt

- Neues C++-Widget UServerStatusWidget (UI/ServerStatusWidget.h/.cpp), abgeleitet von UUserWidget — gleicher Stil wie ChatWidget.

- Zwei UImage per meta=(BindWidget): ImgHermes, ImgQwen (Kreis-Brush im WBP). Optional TxtHermes/TxtQwen per BindWidgetOptional.

- Farben als UPROPERTY im Editor anpassbar (OnlineColor gruen / OfflineColor rot), Poll-Intervall ueber UpdateInterval.

- BlueprintPure-Getter IsHermesOnline() / IsQwenOnline() fuer optionale Blueprint-Bindings.

- Einbau in ALLM\_NPCCharacter::BeginPlay ueber eine TSubclassOf\<UServerStatusWidget\> ServerStatusWidgetClass-Property, AddToViewport(0) (Z-Order unter dem Chat) — dauerhaft sichtbar.

##### Bug & Fix: Kreise wurden nie gruen

###### Problem

Erste Version las die Startup-Flags der Manager (IsServerReady() / IsServerRunning()). Die werden aber nur **einmalig beim Start** ueber einen Subsystem-Timer (PollServerReady) gesetzt — und bleiben false haengen, wenn der Poll nicht durchlaeuft (z.B. GetWorld() im Initialize noch null, oder die 15 Versuche reichen nicht). Der Server beantwortet derweil ganz normal Chat-Anfragen (die haengen nicht am Flag) → Kreis blieb ewig rot, obwohl online.

###### Loesung

Das Widget pingt jetzt **selbst live** GET /health an beide Ports (alle UpdateInterval, Default 1.0s, Timeout 2s):

- HTTP 200 → online (gruen)

- 503 (laedt noch) / kein Response / Timeout → offline (rot)

Damit zeigt es den **echten aktuellen** Zustand: wird gruen sobald der Server bereit ist, und rot wenn er spaeter abstuerzt. Unabhaengig von den Manager-Flags. Ports kommen aus GetPort() der Manager (Fallback 8080/8081).

##### UMG-Setup (Designer)

1. Build → WBP von ServerStatusWidget ableiten.

2. Zwei Image-Widgets exakt ImgHermes / ImgQwen (Kreis-Brush).

3. Optional zwei TextBlocks TxtHermes / TxtQwen.

4. ServerStatusWidgetClass am Player-BP auf das WBP setzen.

##### Offen

- Optionaler „Glow"-Effekt ueber Emissive-Material (aktuell nur Einfaerbung).

- Health-Pings laufen auch wenn das Widget unsichtbar ist — bei Bedarf nur bei Sichtbarkeit pollen.

