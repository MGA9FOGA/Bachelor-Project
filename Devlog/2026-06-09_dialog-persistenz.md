# Devlog – Dialog-Persistenz pro NPC (2026-06-09)

# 18-Eintrag: Gespraechsverlauf pro NPC (Laufzeit)

##### Ziel

Jeder NPC soll seinen eigenen Gespraechsverlauf behalten. Spricht der Spieler einen NPC erneut an, wird der fruehere Verlauf wieder angezeigt — und der NPC bleibt inhaltlich im Kontext.

##### Umgesetzt

- Quelle der Wahrheit ist die bereits vorhandene (bisher ungenutzte) FDialogData.DialogHistory im ECS **jedes NPCs**.

- GetComponent\<FDialogData\>() zum ECS-Dispatch ergaenzt (hatte gefehlt), Shortcut ANPCCharacter::GetDialog() dazu.

- ANPCCharacter::AddDialogTurn(text, bIsNPCResponse) schreibt jeden Turn und deckelt auf MAX\_DIALOG\_HISTORY = 10 (aelteste zuerst raus).

- ChatWidget schreibt beim Reden in den NPC:

  - Spieler-Zeile in SendMessage()

  - NPC-Antwort in OnLLMResponse() (gebuendelt mit ApplyDialogueResult)

- OpenForNPC() ruft jetzt LoadHistoryFromNPC() statt ClearHistory() — laedt den Verlauf des NPCs zurueck in UI + m\_conversationHistory.

##### Design-Entscheidung: kleines LLM-Fenster

Die Server-Kontextfenster sind winzig (Hermes 1024 / Qwen 512 Token). Darum geht **nicht** der volle Verlauf an den LLM, sondern nur die **letzten 6** **Eintraege** (~3 Wortwechsel). Die UI zeigt weiterhin den vollen gespeicherten Verlauf (bis 10 Turns).

##### Reichweite

Nur **Laufzeit** — der Verlauf lebt im ECS und geht beim Beenden des Spiels verloren. Persistenz ueber Spielstaende (Serialisierung) ist bewusst noch nicht umgesetzt (eigener Schritt).

##### Geaenderte Dateien

- NPC/Components/NPC\_ECS.h — FDialogData im GetComponent-Dispatch

- NPC/NPCCharacter.h / .cpp — GetDialog(), AddDialogTurn()

- UI/ChatWidget.h / .cpp — LoadHistoryFromNPC(), Schreiben/Laden, LLM-Verlaufsfenster (6)

##### Verhalten

- NPC A ansprechen → reden → weglaufen → wieder ansprechen ⇒ alter Verlauf erscheint wieder.

- NPC B hat seinen eigenen Verlauf (eigenes ECS).

##### Offen

- Persistenz ueber Spielstaende (Save/Load) — Serialisierung von FDialogData.

- Aeltere Turns ins Memory-System zusammenfassen, statt sie nur zu verwerfen.

