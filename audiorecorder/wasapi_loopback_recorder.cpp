#include <windows.h>
#include <mmreg.h>
// wasapi_loopback_recorder.cpp
// Minimal WASAPI loopback audio recorder for Windows
// Records system audio and saves to output.wav
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <fstream>
#include <vector>
#include <iostream>
#include <csignal>
#include <atomic>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "winmm.lib")

void WriteWavHeader(std::ofstream& out, WAVEFORMATEX* wfx, int dataLength) {
    out.write("RIFF", 4);
    int32_t chunkSize = 36 + dataLength;
    out.write(reinterpret_cast<const char*>(&chunkSize), 4);
    out.write("WAVEfmt ", 8);
    int32_t subchunk1Size = 16;
    out.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
    // Write PCM format header (16-bit)
    WAVEFORMATEX wfx_pcm = *wfx;
    wfx_pcm.wFormatTag = 1; // PCM
    wfx_pcm.wBitsPerSample = 16;
    wfx_pcm.nBlockAlign = wfx_pcm.nChannels * wfx_pcm.wBitsPerSample / 8;
    wfx_pcm.nAvgBytesPerSec = wfx_pcm.nSamplesPerSec * wfx_pcm.nBlockAlign;
    out.write(reinterpret_cast<const char*>(&wfx_pcm), 16);
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&dataLength), 4);
}

// --- Signal handler for Ctrl+C ---

volatile std::atomic_bool g_shouldStop(false);
void SignalHandler(int signum) {
    std::cout << "\nSIGINT received, stopping recording..." << std::endl;
    g_shouldStop = true;
}

int main() {
    signal(SIGINT, SignalHandler);
    HRESULT hr;
    CoInitialize(nullptr);

    std::cout << "Checking for WASAPI loopback support...\n";
    IMMDeviceEnumerator* pEnumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) { std::cerr << "Failed to create device enumerator\n"; return 1; }

    // Get default output (loopback) device
    IMMDevice* pDeviceOut = nullptr;
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDeviceOut);
    if (FAILED(hr)) { std::cerr << "Failed to get default audio endpoint\n"; return 1; }

    // Get default input (microphone) device
    IMMDevice* pDeviceIn = nullptr;
    hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDeviceIn);
    if (FAILED(hr)) { std::cerr << "Failed to get default input endpoint\n"; return 1; }

    // Activate output audio client
    IAudioClient* pAudioClientOut = nullptr;
    hr = pDeviceOut->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClientOut);
    if (FAILED(hr)) { std::cerr << "Failed to activate output audio client\n"; return 1; }

    // Activate input audio client
    IAudioClient* pAudioClientIn = nullptr;
    hr = pDeviceIn->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClientIn);
    if (FAILED(hr)) { std::cerr << "Failed to activate input audio client\n"; return 1; }

    // Get mix format (output)
    WAVEFORMATEX* pwfxOut = nullptr;
    hr = pAudioClientOut->GetMixFormat(&pwfxOut);
    if (FAILED(hr)) { std::cerr << "Failed to get output mix format\n"; return 1; }
    std::cout << "Output format: wFormatTag=" << pwfxOut->wFormatTag
              << ", wBitsPerSample=" << pwfxOut->wBitsPerSample
              << ", nChannels=" << pwfxOut->nChannels << std::endl;

    // Get mix format (input)
    WAVEFORMATEX* pwfxIn = nullptr;
    hr = pAudioClientIn->GetMixFormat(&pwfxIn);
    if (FAILED(hr)) { std::cerr << "Failed to get input mix format\n"; return 1; }
    std::cout << "Input format: wFormatTag=" << pwfxIn->wFormatTag
              << ", wBitsPerSample=" << pwfxIn->wBitsPerSample
              << ", nChannels=" << pwfxIn->nChannels << std::endl;

    // Use output format for recording (assume both are compatible)
    REFERENCE_TIME hnsRequestedDuration = 10 * 10000000; // 10 seconds
    hr = pAudioClientOut->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                 AUDCLNT_STREAMFLAGS_LOOPBACK, hnsRequestedDuration, 0, pwfxOut, nullptr);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize output audio client in loopback mode. WASAPI loopback not available.\n";
        return 1;
    }
    hr = pAudioClientIn->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                 0, hnsRequestedDuration, 0, pwfxOut, nullptr);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize input audio client.\n";
        return 1;
    }
    std::cout << "WASAPI loopback and microphone will be used for recording.\n";

    UINT32 bufferFrameCountOut;
    hr = pAudioClientOut->GetBufferSize(&bufferFrameCountOut);
    if (FAILED(hr)) { std::cerr << "Failed to get output buffer size\n"; return 1; }
    UINT32 bufferFrameCountIn;
    hr = pAudioClientIn->GetBufferSize(&bufferFrameCountIn);
    if (FAILED(hr)) { std::cerr << "Failed to get input buffer size\n"; return 1; }

    IAudioCaptureClient* pCaptureClientOut = nullptr;
    hr = pAudioClientOut->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClientOut);
    if (FAILED(hr)) { std::cerr << "Failed to get output capture client\n"; return 1; }
    IAudioCaptureClient* pCaptureClientIn = nullptr;
    hr = pAudioClientIn->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClientIn);
    if (FAILED(hr)) { std::cerr << "Failed to get input capture client\n"; return 1; }

    hr = pAudioClientOut->Start();
    if (FAILED(hr)) { std::cerr << "Failed to start output audio client\n"; return 1; }
    hr = pAudioClientIn->Start();
    if (FAILED(hr)) { std::cerr << "Failed to start input audio client\n"; return 1; }

    // --- Ensure 'audio' subfolder exists and generate timestamped filename ---
    CreateDirectoryA("audio", nullptr); // Create 'audio' folder if it doesn't exist
    SYSTEMTIME st;
    GetLocalTime(&st);
    char filename[160];
    snprintf(filename, sizeof(filename), "audio/output_%04d-%02d-%02d_%02d-%02d-%02d.wav",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::ofstream out(filename, std::ios::binary);
    std::vector<short> pcm16Data;

    std::cout << "Recording... Press Ctrl+C to stop." << std::endl;
    while (!g_shouldStop) {
        // OUT (loopback)
        UINT32 packetLengthOut = 0;
        hr = pCaptureClientOut->GetNextPacketSize(&packetLengthOut);
        // IN (mic)
        UINT32 packetLengthIn = 0;
        hr = pCaptureClientIn->GetNextPacketSize(&packetLengthIn);

        while (packetLengthOut != 0 || packetLengthIn != 0) {
            // OUT
            BYTE* pDataOut = nullptr;
            UINT32 numFramesOut = 0;
            DWORD flagsOut = 0;
            if (packetLengthOut != 0) {
                hr = pCaptureClientOut->GetBuffer(&pDataOut, &numFramesOut, &flagsOut, nullptr, nullptr);
                std::cout << "[OUT] frames=" << numFramesOut
                          << " | FormatTag=" << pwfxOut->wFormatTag
                          << ", BitsPerSample=" << pwfxOut->wBitsPerSample
                          << ", Channels=" << pwfxOut->nChannels
                          << ", flags=0x" << std::hex << flagsOut << std::dec << std::endl;
                if (numFramesOut == 0) std::cout << "[OUT] WARNING: zero frames delivered!" << std::endl;
            }
            // IN
            BYTE* pDataIn = nullptr;
            UINT32 numFramesIn = 0;
            DWORD flagsIn = 0;
            if (packetLengthIn != 0) {
                hr = pCaptureClientIn->GetBuffer(&pDataIn, &numFramesIn, &flagsIn, nullptr, nullptr);
                std::cout << "[IN] frames=" << numFramesIn
                          << " | FormatTag=" << pwfxIn->wFormatTag
                          << ", BitsPerSample=" << pwfxIn->wBitsPerSample
                          << ", Channels=" << pwfxIn->nChannels
                          << ", flags=0x" << std::hex << flagsIn << std::dec << std::endl;
                if (numFramesIn == 0) std::cout << "[IN] WARNING: zero frames delivered!" << std::endl;
            }
            // Format mismatch warning
            if (pwfxOut->nChannels != pwfxIn->nChannels || pwfxOut->nSamplesPerSec != pwfxIn->nSamplesPerSec) {
                std::cout << "[WARN] Output/Input format mismatch: OutChannels=" << pwfxOut->nChannels
                          << ", InChannels=" << pwfxIn->nChannels
                          << ", OutSampleRate=" << pwfxOut->nSamplesPerSec
                          << ", InSampleRate=" << pwfxIn->nSamplesPerSec << std::endl;
            }

            // Use maximum frames available from either stream for mixing
            UINT32 maxFrames = numFramesOut > numFramesIn ? numFramesOut : numFramesIn;
            if (maxFrames == 0) maxFrames = (numFramesOut > 0) ? numFramesOut : numFramesIn;

            std::vector<short> outSamples(maxFrames * pwfxOut->nChannels, 0);
            std::vector<short> inSamples(maxFrames * pwfxOut->nChannels, 0);
            // OUT conversion
            if (packetLengthOut != 0 && pDataOut) {
                UINT32 outCount = numFramesOut * pwfxOut->nChannels;
                if (pwfxOut->wFormatTag == 3 && pwfxOut->wBitsPerSample == 32) {
                    float* floatSamples = (float*)pDataOut;
                    for (UINT32 i = 0; i < outCount; ++i) {
                        int val = static_cast<int>(floatSamples[i] * 32767.0f);
                        if (val > 32767) val = 32767;
                        if (val < -32768) val = -32768;
                        outSamples[i] = (short)val;
                    }
                } else if (pwfxOut->wFormatTag == 1 && pwfxOut->wBitsPerSample == 16) {
                    short* samples = (short*)pDataOut;
                    for (UINT32 i = 0; i < outCount; ++i) outSamples[i] = samples[i];
                } else if (pwfxOut->wFormatTag == 65534) {
                    WAVEFORMATEXTENSIBLE* wfexOut = (WAVEFORMATEXTENSIBLE*)pwfxOut;
                    if (wfexOut->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT && pwfxOut->wBitsPerSample == 32) {
                        float* floatSamples = (float*)pDataOut;
                        for (UINT32 i = 0; i < outCount; ++i) {
                            int val = static_cast<int>(floatSamples[i] * 32767.0f);
                            if (val > 32767) val = 32767;
                            if (val < -32768) val = -32768;
                            outSamples[i] = (short)val;
                        }
                    } else if (wfexOut->SubFormat == KSDATAFORMAT_SUBTYPE_PCM && pwfxOut->wBitsPerSample == 16) {
                        short* samples = (short*)pDataOut;
                        for (UINT32 i = 0; i < outCount; ++i) outSamples[i] = samples[i];
                    } else {
                        std::cout << "[OUT] Unsupported extensible format!" << std::endl;
                    }
                }
            }
            // IN conversion
            if (packetLengthIn != 0 && pDataIn) {
                UINT32 inCount = numFramesIn * pwfxIn->nChannels;
                if (pwfxIn->wFormatTag == 3 && pwfxIn->wBitsPerSample == 32) {
                    float* floatSamples = (float*)pDataIn;
                    for (UINT32 i = 0; i < inCount; ++i) {
                        int val = static_cast<int>(floatSamples[i] * 32767.0f);
                        if (val > 32767) val = 32767;
                        if (val < -32768) val = -32768;
                        inSamples[i] = (short)val;
                    }
                } else if (pwfxIn->wFormatTag == 1 && pwfxIn->wBitsPerSample == 16) {
                    short* samples = (short*)pDataIn;
                    for (UINT32 i = 0; i < inCount; ++i) inSamples[i] = samples[i];
                } else if (pwfxIn->wFormatTag == 65534) {
                    WAVEFORMATEXTENSIBLE* wfexIn = (WAVEFORMATEXTENSIBLE*)pwfxIn;
                    if (wfexIn->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT && pwfxIn->wBitsPerSample == 32) {
                        float* floatSamples = (float*)pDataIn;
                        for (UINT32 i = 0; i < inCount; ++i) {
                            int val = static_cast<int>(floatSamples[i] * 32767.0f);
                            if (val > 32767) val = 32767;
                            if (val < -32768) val = -32768;
                            inSamples[i] = (short)val;
                        }
                    } else if (wfexIn->SubFormat == KSDATAFORMAT_SUBTYPE_PCM && pwfxIn->wBitsPerSample == 16) {
                        short* samples = (short*)pDataIn;
                        for (UINT32 i = 0; i < inCount; ++i) inSamples[i] = samples[i];
                    } else {
                        std::cout << "[IN] Unsupported extensible format!" << std::endl;
                    }
                }
            }
            // Mix (simple sum, clamp)
            if (numFramesOut > 0 && numFramesIn > 0) {
                // Both streams have data: mix
                for (UINT32 i = 0; i < maxFrames * pwfxOut->nChannels; ++i) {
                    int mixed = (int)outSamples[i] + (int)inSamples[i];
                    if (mixed > 32767) mixed = 32767;
                    if (mixed < -32768) mixed = -32768;
                    pcm16Data.push_back((short)mixed);
                }
            } else if (numFramesOut > 0) {
                // Only output stream has data
                for (UINT32 i = 0; i < numFramesOut * pwfxOut->nChannels; ++i) {
                    pcm16Data.push_back(outSamples[i]);
                }
            } else if (numFramesIn > 0) {
                // Only input stream has data
                for (UINT32 i = 0; i < numFramesIn * pwfxOut->nChannels; ++i) {
                    pcm16Data.push_back(inSamples[i]);
                }
            }
            if (packetLengthOut != 0) hr = pCaptureClientOut->ReleaseBuffer(numFramesOut);
            if (packetLengthIn != 0) hr = pCaptureClientIn->ReleaseBuffer(numFramesIn);
            hr = pCaptureClientOut->GetNextPacketSize(&packetLengthOut);
            hr = pCaptureClientIn->GetNextPacketSize(&packetLengthIn);
        }
        Sleep(10);
    }

    hr = pAudioClientOut->Stop();
    hr = pAudioClientIn->Stop();


    // --- Postprocess: Smooth PCM data to reduce clicking ---
    std::vector<short> smoothedData(pcm16Data.size());
    const int window = 8; // Moving average window size (stronger smoothing)
    for (size_t i = 0; i < pcm16Data.size(); ++i) {
        int sum = 0;
        int count = 0;
        for (int w = -window; w <= window; ++w) {
            size_t idx = i + w;
            if (idx < pcm16Data.size()) {
                sum += pcm16Data[idx];
                ++count;
            }
        }
        smoothedData[i] = static_cast<short>(sum / count);
    }

    int pcmDataBytes = (int)smoothedData.size() * sizeof(short);
    WriteWavHeader(out, pwfxOut, pcmDataBytes);
    out.write(reinterpret_cast<const char*>(smoothedData.data()), pcmDataBytes);
    out.close();

    CoTaskMemFree(pwfxOut);
    CoTaskMemFree(pwfxIn);
    pCaptureClientOut->Release();
    pCaptureClientIn->Release();
    pAudioClientOut->Release();
    pAudioClientIn->Release();
    pDeviceOut->Release();
    pDeviceIn->Release();
    pEnumerator->Release();
    CoUninitialize();

    std::cout << "Done. Saved to output.wav\n";
    return 0;
}
