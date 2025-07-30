# Meeting Audio Recording and Transcription Workflow

This project provides a robust workflow for recording, splitting, and transcribing meeting audio, with the final step of summarizing the transcript using Copilot agent.

## Step-by-Step Usage



1. **Record the Meeting Audio**
   - Use the WASAPI loopback recorder to capture all PC and microphone audio during your meeting.
   - **Note:** `wasapi_loopback_recorder.exe` only works on Windows, as it uses Windows API functions for audio capture.
   - The recorder currently produces some clicking artifacts in the audio, but this did not noticeably affect the transcription results with Whisper.
   - To build the wasapi_loopback_recorder browse to the audiorecorder folder and configure and build the CMakeLists.txt. Afterwards copy the wasapi_loopback_recorder.exe from the .\build\Debug\ or .\build\Release\ folder into the root folder depending on your build configuration.
   - You can use any type of recording software you like as long as you make sure to create the recorded audio files in the following pattern: e.g., `audio/output_2025-07-24_13-00-45.wav`
   - I used the wasapi_loopback_recorder because i needed to record my pc audo and the microphone input at the same time. Other tools like audacity or similar can do this aswell.
   - The rest of the tooling (Python scripts, ffmpeg, Whisper) can also be used on Linux and possibly macOS. You will need to download ffmpeg separately for your operating system and put the executables into the scripts directory.
   - Run:
     ```
     ./wasapi_loopback_recorder.exe
     ```
   - The output will be a timestamped WAV file in the `audio` subfolder (e.g., `audio/output_2025-07-24_13-00-45.wav`).
   - To stop and finalize the recording, press `Ctrl+C` in the terminal where the recorder is running.

2. **Split the Audio File on Silence**
   - Use the provided Python script to split the recorded audio into smaller chunks at silence points.
   - Run:
     ```
     python scripts/ffmpeg_split_latest.py
     ```
   - This will create a new subfolder in `audio` (e.g., `audio/output_2025-07-24_13-00-45/`) containing the split chunk files.

3. **Transcribe the Audio Chunks**
   - Use the transcription script to transcribe all audio chunks using Whisper.
   - Run:
     ```
     python scripts/transcribe_splits_latest.py
     ```
   - By default, transcription is in German. To transcribe in English, add `--english`:
     ```
     python scripts/transcribe_splits_latest.py --english
     ```
   - The resulting transcript will be saved as a text file in the `audiologs` folder (e.g., `audiologs/dialog_output_2025-07-24_13-00-45.txt`).

4. **Summarize the Audiolog**
   - Use the Copilot agent to summarize the transcript and generate your final meeting notes.
   - Open the transcript from the `audiologs` folder and use Copilot's summarization capabilities as needed.

---

**Tip:**
- Make sure you have Python, ffmpeg, and the required Python packages (including `openai-whisper`) installed.
- For best results, ensure your audio is clear and background noise is minimized during recording.
