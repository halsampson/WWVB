#pragma once

#include <windows.h>
#include <mmsystem.h>

const double SampleHz = 191996.730;          // Audio sampling rate
  // 0.690 72F
  // 0.784 70F
  // Use 1PPScalib to calibrate
  // Use 'f' key to see estimated error

void setupAudioIn(const char* deviceName, void (*)(WAVEHDR* wh));
void startWaveIn();
void waveInReady();
void stopWaveIn();