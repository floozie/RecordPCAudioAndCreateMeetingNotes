# Anforderungen: Audio Recording, Whisper, Transcribe

- Split FFMpeg und Transcribe mit Whisper (neueste Version):
  - Transkriptionsskript nimmt Audiodatei als Parameter.
  - Nach erfolgreicher Transkription wird die Audiodatei in den Papierkorb verschoben (wiederherstellbar), außer ein Kommando-Parameter verhindert dies.
  - Ohne Parameter wird die neueste Audiodatei oder die zuletzt gesplitteten Audiodateien verwendet.
- Prozess: Warnung, wenn ein Teams-Meeting, WhatsApp-Call o.ä. erkannt wird, damit Audio Recorder gestartet wird.
- Whisper soll zwischen verschiedenen Stimmen unterscheiden können.
