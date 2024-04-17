#pragma once

const double SampleHz = 191996.655;              // Audio sampling rate measured by long recording of 1PPS
  // TODO: automate SampleHz measurement from 1PPS or from WWVB phase drift (long term averaged)

const int BufferSamples = int(SampleHz + 0.5);
extern short wavInBuf[2][BufferSamples];

void setupAudioIn(const char* deviceName, void (*)(int b));
void startWaveIn();
bool waveInReady();
  // see also MM_WOM_DONE: https://learn.microsoft.com/en-us/windows/win32/multimedia/processing-the-mm-wom-done-message