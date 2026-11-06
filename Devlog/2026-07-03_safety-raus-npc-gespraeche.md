# Devlog – 2026-07-03

Aufräumen und ein neues Lebendigkeits-Feature: das ungenutzte Safety-System ist
raus, und NPCs führen jetzt untereinander kurze Gespräche – inklusive Gerüchte-
Weitergabe. (Fortsetzung nach #70.)

---

# 71-Eintrag: Safety-System entfernt

##### Ausgangslage

`Safety` war ein totes Bedürfnis: der Wert stand immer auf 1.0, `FeelsUnsafe()`
triggerte nie (es gibt bewusst keine Gefahren), und trotzdem zog sich der
Unsafe-Pfad durch Needs, Dialog-Prompts und Debug.

##### Umgesetzt

- `FNeedsData`: `Safety`, `SafetyThreshhold`, `FeelsUnsafe()` entfernt.
- `NPCDataAsset`: `BaseSafety` entfernt.
- Alle Nutzungen raus: ECS-Clamp, Debug-Print, Init (mit/ohne DataAsset),
  Hermes-Prompt („You feel unsafe") und `GenerateNeedsPrompt` (Safety-Zeile).
- Die Prompts sagen jetzt nur noch „You feel well", wenn weder hungrig noch müde.

##### Hinweis

Falls später doch Bedrohungen kommen, lässt sich das Bedürfnis über denselben
Weg wieder anhängen – der Neuplanungs-Hook (`OnLearnedInformation`) bleibt ja.

---

# 72-Eintrag: NPC-zu-NPC-Gespräche (+ Gossip)

##### Ziel

NPCs sollen sich untereinander unterhalten – das macht die Welt spürbar
lebendiger – und dabei Infos weitergeben (Gerüchte).

##### Umgesetzt

Baut komplett auf der vorhandenen Talk-Infrastruktur auf (kein neues System):

- **Erkennung** (`Tick`): ein *freier* NPC (schlendert/idle, redet nicht,
  Cooldown ok) sucht per `TActorIterator` einen nahen, ebenfalls freien NPC
  (`SocialRadius`, Default 250 cm).
- **Start** (`StartConversationWith`): beide rufen `SetTalking(true, other)` →
  stehen still, drehen sich über `BTTask_Talk` zueinander, spielen die Rede-Idle.
- **Gestik**: ein Timer lässt sie **abwechselnd** die Antwort-Animation spielen
  (`PlayTalkReply`/`EndTalkReply` im Ping-Pong, alle 2 s) → „wer spricht gerade".
- **Gossip**: jeder gibt seine wichtigste Erinnerung an den anderen weiter –
  über `OnLearnedInformation(..., bReplan=false)`. **Kein LLM-Call pro Schnack**;
  die Info fließt beim nächsten regulären Plan (täglich/Quest) ein. So verbreiten
  sich Gerüchte durchs Dorf.
- **Ende** nach ~6 s (variiert), danach `SocialCooldown` (20 s).

##### Robustheit

- **Spieler hat Vorrang:** sprichst du einen chattenden NPC an, wird das
  NPC-Gespräch zuerst sauber beendet.
- Ein weggehender Spieler bricht ein NPC-Gespräch **nicht** ab
  (`OnInteractionSphereEndOverlap` prüft auf `m_conversationPartner`).
- `EndConversation` beendet beide Seiten rekursionssicher; stirbt ein NPC mitten
  im Gespräch (`EndPlay`), wird der Partner freigegeben.

##### Editor

Pro NPC unter *AI|Social*: `bEnableNPCConversations`, `SocialRadius`,
`SocialChatDuration`, `SocialCooldown`. Nutzt dieselben `TalkIdleMontage` /
`TalkReplyMontage` wie das Spielergespräch.

---

# Nebenbei

- **`OnLearnedInformation`** hat jetzt einen `bReplan`-Schalter (Default true) –
  Gossip nutzt `false`, der Spieler-Quest-Pfad weiter `true`.
- **`MaxWalkSpeed = 165`** im NPC-Konstruktor (passt zu einer Walk-Anim, kein
  Foot-Sliding). Blendspace also 0=Idle, 165=Walk.
- **Talk-Montagen-Stop hart:** beim Schließen des Chats stoppt `StopAllMontages`
  alle (auch die durch den Selbst-Loop entstandenen) Instanzen sofort – vorher
  spielte eine Talk-Montage nach dem Schließen bis zu ~1 s zu Ende („keine
  Lauf-Animation").

---

# Aktueller Stand

Das Bedürfnis-System ist auf Hunger + Erschöpfung reduziert. NPCs leben nicht nur
ihren Tagesablauf, sondern bleiben auch mal stehen, um miteinander zu reden, und
geben dabei Gerüchte weiter, die (beim nächsten Plan) ihren Ablauf beeinflussen.
Der Spieler kann jederzeit dazwischengehen.
