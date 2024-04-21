#pragma once

const double SampleHz = 191996.635;          // Audio sampling rate
  // Use 1PPScalib to calibrate
  // Use 'f' key to see estimated error

const int BufferSamples = int(SampleHz + 0.5);
extern short wavInBuf[2][BufferSamples];

void setupAudioIn(const char* deviceName, void (*)(int b, int samplesRecorded));
void startWaveIn();
bool waveInReady();
  // see also MM_WOM_DONE: https://learn.microsoft.com/en-us/windows/win32/multimedia/processing-the-mm-wom-done-message
void stopWaveIn();