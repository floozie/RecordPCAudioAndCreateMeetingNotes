import os
import sys
import datetime
import sounddevice as sd
import numpy as np
import whisper
import threading
import queue
import time

LANGUAGE = "de"
MODEL_SIZE = "large"
SAMPLE_RATE = 16000
CHANNELS = 1
BLOCKSIZE = 1024
MIN_CHUNK_SECONDS = 2    # Ignore chunks shorter than this
CHUNK_SECONDS = 5        # Buffer at least 5 seconds before transcribing
MAX_CHUNK_SECONDS = 15   # Force transcription after 15 seconds
SILENCE_THRESHOLD = 0.01
SILENCE_DURATION = 1.0

DICTATIONS_DIR = os.path.join(os.path.dirname(__file__), '..', 'dictations')
if not os.path.exists(DICTATIONS_DIR):
    os.makedirs(DICTATIONS_DIR)
filename = f"dictation_{datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')}.txt"
filepath = os.path.join(DICTATIONS_DIR, filename)

print("Lade Whisper-Modell...")
model = whisper.load_model(MODEL_SIZE)
print("Whisper-Modell geladen.")
print(f"Sprich jetzt. Beende mit Ctrl+C. Ergebnis wird in {filepath} geschrieben.")

buffer = []
silence_buffer = []
silence_samples = int(SAMPLE_RATE * SILENCE_DURATION)
chunk_blocks = int((SAMPLE_RATE * CHUNK_SECONDS) // BLOCKSIZE)
max_buffer_blocks = int((SAMPLE_RATE * MAX_CHUNK_SECONDS) // BLOCKSIZE)
audio_queue = queue.Queue()
transcription_done = threading.Event()
exit_flag = threading.Event()

# Worker thread for transcription
def transcribe_worker():
    MIN_AVG_VOLUME = 0.0005  # Maximum sensitivity for quiet speech
    with open(filepath, 'a', encoding='utf-8') as f:
        while True:
            chunk = audio_queue.get()
            if chunk is None:
                break
            audio = np.concatenate(chunk, axis=0)
            audio = np.ravel(audio)
            duration = len(audio) / SAMPLE_RATE
            avg_volume = np.abs(audio).mean()
            if duration < MIN_CHUNK_SECONDS:
                print(f"Chunk zu kurz ({duration:.2f}s), wird ignoriert.")
                transcription_done.set()
                continue
            print(f"Chunk avg amplitude: {avg_volume:.4f}")
            if avg_volume < MIN_AVG_VOLUME:
                print(f"Chunk zu leise (avg={avg_volume:.4f}), wird ignoriert.")
                transcription_done.set()
                continue
            print(f"Transcription running... (Chunk duration: {duration:.2f}s, avg={avg_volume:.4f})")
            result = model.transcribe(audio.astype(np.float32), language=LANGUAGE, fp16=False)
            text = result['text'].strip()
            if text:
                print(f"Transcribed: {text}")
                if text.lower() in ["vielen dank", "", "thank you"]:
                    print("Warning: Whisper only recognized a default phrase. Chunk was probably too short or too quiet.")
                f.write(text + '\n')
                f.flush()
                os.fsync(f.fileno())
            transcription_done.set()

transcribe_thread = threading.Thread(target=transcribe_worker)
transcribe_thread.start()

try:
    with sd.InputStream(samplerate=SAMPLE_RATE, channels=CHANNELS, blocksize=BLOCKSIZE, dtype='float32') as stream:
        last_check = time.time()
        while not exit_flag.is_set():
            data, _ = stream.read(BLOCKSIZE)
            buffer.append(data)
            flat = np.abs(data).mean()
            silence_buffer.append(flat < SILENCE_THRESHOLD)
            # Transcribe if silence detected and buffer is long enough
            if len(silence_buffer) > silence_samples // BLOCKSIZE:
                if all(silence_buffer[-(silence_samples // BLOCKSIZE):]) and len(buffer) >= chunk_blocks:
                    audio_queue.put(buffer.copy())
                    buffer = []
                    silence_buffer = []
            # Force transcription if buffer too large
            if len(buffer) >= max_buffer_blocks:
                print("Maximale Chunk-Länge erreicht, erzwungene Transkription...")
                audio_queue.put(buffer.copy())
                buffer = []
                silence_buffer = []
            # Only print status once after each transcription
            if transcription_done.is_set():
                print("Transcription is up to date. You can safely exit.")
                transcription_done.clear()
            # Check for KeyboardInterrupt every 0.2s
            if time.time() - last_check > 0.2:
                last_check = time.time()
                if exit_flag.is_set():
                    break
except KeyboardInterrupt:
    print("Stopped. Waiting for last transcription...")
    exit_flag.set()
    if buffer:
        audio_queue.put(buffer.copy())
    audio_queue.put(None)
    transcribe_thread.join()
    print("All transcriptions completed.")
    sys.exit(0)
except Exception as e:
    print(f"Error: {e}")
    exit_flag.set()
    audio_queue.put(None)
    transcribe_thread.join()
    sys.exit(1)
