import os
import glob
import whisper
import sys

AUDIO_DIR = os.path.join(os.path.dirname(__file__), '..', 'audio')
LOG_DIR = os.path.join(os.path.dirname(__file__), '..', 'audiologs')
os.makedirs(LOG_DIR, exist_ok=True)

# Find latest timestamped split folder
subfolders = [f for f in glob.glob(os.path.join(AUDIO_DIR, 'output_*')) if os.path.isdir(f)]
if not subfolders:
    print('No timestamped subfolders found in', AUDIO_DIR)
    sys.exit(1)
latest_folder = max(subfolders, key=os.path.getmtime)
print('Using latest split folder:', latest_folder)

# Find all chunk wav files in order
chunks = sorted(glob.glob(os.path.join(latest_folder, '*_chunk*.wav')))
if not chunks:
    print('No chunk wav files found in', latest_folder)
    sys.exit(1)
print(f'Found {len(chunks)} chunks:')
for c in chunks:
    print('  ', os.path.basename(c))

# Command line: --english for English transcription
lang = 'de'
if len(sys.argv) > 1 and sys.argv[1].lower() == '--english':
    lang = 'en'
    print('Transcribing in English mode.')
else:
    print('Transcribing in German mode.')

model = whisper.load_model('large')
all_text = ''
for idx, chunk in enumerate(chunks):
    print(f'\nTranscribing chunk {idx+1}/{len(chunks)}: {os.path.basename(chunk)}')
    result = model.transcribe(chunk, language=lang, task='transcribe', temperature=0, verbose=True)
    all_text += result['text'] + '\n'

# Save audiolog
base = os.path.basename(latest_folder)
log_filename = f'dialog_{base}.txt'
log_path = os.path.join(LOG_DIR, log_filename)
with open(log_path, 'w', encoding='utf-8') as f:
    f.write(all_text)
print('\nSaved audiolog to', log_path)
