
import os
import glob
import sys
import subprocess
import re

# Parameters for silence detection
SILENCE_LEN = 1.0  # seconds of silence to trigger a split
SILENCE_THRESH = -35  # dB threshold for silence
MIN_CHUNK_LEN = 5  # minimum chunk length in seconds

AUDIO_DIR = os.path.join(os.path.dirname(__file__), '..', 'audio')

# Find latest audio file
wav_files = glob.glob(os.path.join(AUDIO_DIR, 'output_*.wav'))
if not wav_files:
    print('No audio files found in', AUDIO_DIR)
    sys.exit(1)
latest_file = max(wav_files, key=os.path.getmtime)
print('Splitting:', latest_file)

# Prepare output folder
base = os.path.basename(latest_file)
name = base.replace('.wav', '')
split_dir = os.path.join(AUDIO_DIR, name)
os.makedirs(split_dir, exist_ok=True)

# 1st pass: run silencedetect to get silence times
logfile = os.path.join(split_dir, 'silencedetect.log')
ffmpeg_cmd = [
    'ffmpeg',
    '-hide_banner',
    '-i', latest_file,
    '-af', f'silencedetect=noise={SILENCE_THRESH}dB:d={SILENCE_LEN}',
    '-f', 'null', '-'
]
print('Detecting silence...')
with open(logfile, 'w', encoding='utf-8') as logf:
    proc = subprocess.run(ffmpeg_cmd, stdout=logf, stderr=subprocess.STDOUT, text=True)
if proc.returncode != 0:
    print('ffmpeg silencedetect error')
    sys.exit(1)

# Parse silence log for split points
with open(logfile, encoding='utf-8') as f:
    lines = f.readlines()

silence_starts = []
silence_ends = []
for line in lines:
    m = re.search(r'silence_start: (\d+\.\d+)', line)
    if m:
        silence_starts.append(float(m.group(1)))
    m = re.search(r'silence_end: (\d+\.\d+)', line)
    if m:
        silence_ends.append(float(m.group(1)))

# Build split points (start at 0, then every silence_end)
split_points = [0.0] + silence_ends

# Remove splits that would make chunks too short
final_splits = []
for i in range(1, len(split_points)):
    if split_points[i] - split_points[i-1] >= MIN_CHUNK_LEN:
        final_splits.append((split_points[i-1], split_points[i]))
# Add last chunk if needed
if not silence_ends or (split_points[-1] < float(proc.stdout) if proc.stdout else 1e9):
    # Get duration
    ffprobe_cmd = [
        'ffprobe',
        '-v', 'error',
        '-show_entries', 'format=duration',
        '-of', 'default=noprint_wrappers=1:nokey=1',
        latest_file
    ]
    result = subprocess.run(ffprobe_cmd, capture_output=True, text=True)
    duration = float(result.stdout.strip()) if result.returncode == 0 else None
    if duration and (not final_splits or duration - split_points[-1] >= MIN_CHUNK_LEN):
        final_splits.append((split_points[-1], duration))

print(f'Found {len(final_splits)} chunks to extract.')

# 2nd pass: extract chunks
for idx, (start, end) in enumerate(final_splits):
    outname = os.path.join(split_dir, f'{name}_chunk{idx:03d}.wav')
    cmd = [
        'ffmpeg',
        '-hide_banner',
        '-i', latest_file,
        '-ss', str(start),
        '-to', str(end),
        '-c:a', 'pcm_s16le',
        outname
    ]
    print(f'Extracting chunk {idx}: {start:.2f} - {end:.2f} sec')
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        print(f'ffmpeg error on chunk {idx}:', proc.stderr)
        continue
print('Done splitting. Chunks are in', split_dir)
