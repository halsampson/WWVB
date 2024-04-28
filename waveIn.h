#pragma once

const double SampleHz = 191996.730;          // Audio sampling rate
  // 0.690 72F
  // 0.784 70F
  // Use 1PPScalib to calibrate
  // Use 'f' key to see estimated error

const int NUM_WAV_BUFFERS = 3;

const int BufferSamples = int(SampleHz + 0.5);
extern short wavInBuf[NUM_WAV_BUFFERS][BufferSamples];

void setupAudioIn(const char* deviceName, void (*)(int b, int samplesRecorded));
void startWaveIn();
bool waveInReady();
  // see also MM_WOM_DONE: https://learn.microsoft.com/en-us/windows/win32/multimedia/processing-the-mm-wom-done-message
void stopWaveIn();