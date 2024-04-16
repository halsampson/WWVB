#include <windows.h>
#include <stdio.h>
#include <mmsystem.h>
#pragma comment(lib, "Winmm.lib")

const double SampleHz = 191996.71;    // Audio sampling rate measured by recording 1PPS for 100s

const unsigned short MicLevel = 65535;
// TODO: also set Mic Boost
//    MIXERCONTROL_CONTROLTYPE_DECIBELS ??

#define WAV_IN_BUF_MSECS 1000
#define WAV_IN_CHANNELS 1
#define BITS_PER_SAMPLE 16
#define WAV_IN_SAMPLE_HZ 192000  // requested 

const int BufferSamples = int(SampleHz + 0.5);
short wavInBuf[2][BufferSamples * WAV_IN_CHANNELS];  

void (*waveInRdyCallback)(int b);

HWAVEIN hwi;
WAVEHDR wih[2]; 

bool waveInReady() {
  bool waveInReady = false;
   for (int b = 0; b < 2; ++b) {
    if (wih[b].dwFlags & WHDR_DONE || !wih[b].dwFlags) {
      if (wih[b].dwFlags & WHDR_DONE) {  
        (*waveInRdyCallback)(b);
        waveInReady = true;
        waveInUnprepareHeader(hwi, &wih[b], sizeof(WAVEHDR));
      }
      
      wih[b].dwBufferLength = BufferSamples * WAV_IN_CHANNELS * BITS_PER_SAMPLE / 8;
      wih[b].lpData = (LPSTR)&wavInBuf[b];
      MMRESULT res = waveInPrepareHeader(hwi, &wih[b], sizeof(WAVEHDR));
      res = waveInAddBuffer(hwi, &wih[b], sizeof(WAVEHDR));
    }
  }
  return waveInReady;
}

void setMicLevel(int wavInDevID, unsigned short micLevel) {
  MMRESULT result;
  HMIXER hMixer;
  result = mixerOpen(&hMixer, (UINT)wavInDevID, NULL, 0, MIXER_OBJECTF_WAVEIN);

  MIXERLINE ml = {0};
  ml.cbStruct = sizeof(MIXERLINE);
  ml.dwComponentType = MIXERLINE_COMPONENTTYPE_SRC_MICROPHONE;
  result = mixerGetLineInfo((HMIXEROBJ)hMixer, &ml, MIXER_GETLINEINFOF_COMPONENTTYPE);

  MIXERLINECONTROLS mlineControls = {0};            // contains information about the controls of an audio line
  MIXERCONTROL controlArray[8] = {0};
  mlineControls.dwLineID  = ml.dwLineID;      // unique audio line identifier
  mlineControls.cControls = ml.cControls;     // number of controls associated with the line
  mlineControls.pamxctrl  = controlArray;     // points to the first MIXERCONTROL structure to be filled
  mlineControls.cbStruct  = sizeof(MIXERLINECONTROLS);
  mlineControls.cbmxctrl  = sizeof(MIXERCONTROL);
  // Get information on ALL controls associated with the specified audio line
  result = mixerGetLineControls((HMIXEROBJ) hMixer, &mlineControls, MIXER_OBJECTF_MIXER | MIXER_GETLINECONTROLSF_ALL);
  // 0: Mute = AGC??  1: Volume

  MIXERLINECONTROLS mlc = {0};
  MIXERCONTROL mc = {0};
  mlc.cbStruct = sizeof(MIXERLINECONTROLS);
  mlc.dwLineID = ml.dwLineID;
  mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_MUTE;
  mlc.cControls = 1;
  mlc.pamxctrl = &mc;
  mlc.cbmxctrl = sizeof(MIXERCONTROL);
  result = mixerGetLineControls((HMIXEROBJ) hMixer, &mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE);

  MIXERCONTROLDETAILS mcd = {0};
  MIXERCONTROLDETAILS_UNSIGNED mcdu = {0};
  mcdu.dwValue = 0; 
  mcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
  mcd.hwndOwner = 0;
  mcd.dwControlID = mc.dwControlID;
  mcd.paDetails = &mcdu;
  mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
  mcd.cChannels = 1;
  result = mixerSetControlDetails((HMIXEROBJ) hMixer, &mcd, MIXER_SETCONTROLDETAILSF_VALUE);

  mlc.cbStruct = sizeof(MIXERLINECONTROLS);
  mlc.dwLineID = ml.dwLineID;
  mlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
  mlc.cControls = 1;
  mlc.pamxctrl = &mc;
  mlc.cbmxctrl = sizeof(MIXERCONTROL);
  result = mixerGetLineControls((HMIXEROBJ) hMixer, &mlc, MIXER_GETLINECONTROLSF_ONEBYTYPE);

  mcdu.dwValue = micLevel; // 0..65535
  mcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
  mcd.hwndOwner = 0;
  mcd.dwControlID = mc.dwControlID;
  mcd.paDetails = &mcdu;
  mcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
  mcd.cChannels = 1;
  result = mixerSetControlDetails((HMIXEROBJ) hMixer, &mcd, MIXER_SETCONTROLDETAILSF_VALUE);
}

void setupAudioIn(const char* deviceName, void (*waveInRdy)(int b)) {
  waveInRdyCallback = waveInRdy;

  int wavInDevID = -1;
  int numDevs = waveInGetNumDevs();
  for (int devID = 0; devID < numDevs; ++devID) {
    WAVEINCAPS wic;
    if (waveInGetDevCaps(devID, &wic, sizeof(WAVEINCAPS)) == MMSYSERR_NOERROR) {
      // printf("DeviceID %d: %s\n", devID, wic.szPname);
      if (strstr(wic.szPname, deviceName)) {
        wavInDevID = devID;
        break;
      }
    }
  }
  if (wavInDevID == -1) {
    printf("Input %s not found\n", deviceName);
    return;
  }

  WAVEFORMATEX wfx = {WAVE_FORMAT_PCM, WAV_IN_CHANNELS,
                      WAV_IN_SAMPLE_HZ, WAV_IN_SAMPLE_HZ * WAV_IN_CHANNELS * BITS_PER_SAMPLE / 8,
                      WAV_IN_CHANNELS * BITS_PER_SAMPLE / 8, BITS_PER_SAMPLE, 0};  
  MMRESULT res = waveInOpen(&hwi, wavInDevID, &wfx, NULL, 0, WAVE_FORMAT_DIRECT);
  
  setMicLevel(wavInDevID, MicLevel);

  waveInReady();
}

void startWaveIn() {
  MMRESULT res = waveInStart(hwi);
}

void stopWaveIn() {
  waveInStop(hwi);
}