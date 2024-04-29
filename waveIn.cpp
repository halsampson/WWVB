#include "waveIn.h"

#include <stdio.h>
#pragma comment(lib, "Winmm.lib")

const unsigned short MicLevel = 65535;
// TODO: also set Mic Boost
//    MIXERCONTROL_CONTROLTYPE_DECIBELS ??

#define WAV_IN_BUF_MSECS 1000
#define WAV_IN_CHANNELS 1
#define BITS_PER_SAMPLE 16
#define WAV_IN_SAMPLE_HZ 192000  // requested standard nominal value

const int NUM_WAV_BUFFERS = 3;

const int BufferSamples = int(SampleHz + 0.5);

short wavInBuf[NUM_WAV_BUFFERS][BufferSamples];

void (*waveInRdyCallback)(WAVEHDR* wh);

HWAVEIN hwi;
WAVEHDR wih[NUM_WAV_BUFFERS]; 

void waveInReady(WAVEHDR* wh) {
  if (wh->dwFlags & WHDR_DONE || !wh->dwFlags) {
    if (wh->dwFlags & WHDR_DONE) {
      (*waveInRdyCallback)(wh);
      waveInUnprepareHeader(hwi, wh, sizeof(WAVEHDR));
    }
      
    MMRESULT res = waveInPrepareHeader(hwi, wh, sizeof(WAVEHDR));
    res = waveInAddBuffer(hwi, wh, sizeof(WAVEHDR));    
  }
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

void CALLBACK waveInProc(
   HWAVEIN   hwi,
   UINT      uMsg,
   DWORD_PTR dwInstance,
   DWORD_PTR dwParam1,
   DWORD_PTR dwParam2) {
  if (uMsg == WIM_DATA) 
    waveInReady((WAVEHDR*)dwParam1); 
}

void setupAudioIn(const char* deviceName, void (*waveInRdy)(WAVEHDR* wh)) {
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
  MMRESULT res = waveInOpen(&hwi, wavInDevID, &wfx, (DWORD_PTR)(VOID*)waveInProc, 0, CALLBACK_FUNCTION | WAVE_FORMAT_DIRECT);
  
  setMicLevel(wavInDevID, MicLevel);

  for (int b = 0; b < NUM_WAV_BUFFERS; ++b) {
    wih[b].lpData = (LPSTR)wavInBuf[b];
    wih[b].dwBufferLength = BufferSamples * WAV_IN_CHANNELS * BITS_PER_SAMPLE / 8;
    waveInReady(&wih[b]);
  }
}

#define POWER_REQ
HANDLE hPCR;

void startWaveIn() {
  MMRESULT res = waveInStart(hwi);

#ifdef POWER_REQ
  REASON_CONTEXT reason = {POWER_REQUEST_CONTEXT_VERSION, POWER_REQUEST_CONTEXT_SIMPLE_STRING, };
  wchar_t reason_string[] = L"Prevent audio gaps"; // for powercfg -
  reason.Reason.SimpleReasonString = reason_string; 
  HANDLE hPCR = PowerCreateRequest(&reason);
  if (!PowerSetRequest(hPCR, PowerRequestExecutionRequired)) printf("PSR error %d\n", GetLastError());
  if (!PowerSetRequest(hPCR, PowerRequestDisplayRequired)) printf("PSR error %d\n", GetLastError()); // prevents audio gaps OK
  if (!PowerSetRequest(hPCR, PowerRequestSystemRequired)) printf("PSR error %d\n", GetLastError()); // already done by Realtek driver
#endif

 // SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED); // don't sleep, run in background
 // no help - still get audio buffer discontinuities

}

void stopWaveIn() {
  waveInStop(hwi);
#ifdef POWER_REQ
  PowerClearRequest(hPCR, PowerRequestExecutionRequired);
  PowerClearRequest(hPCR, PowerRequestDisplayRequired);
  PowerClearRequest(hPCR, PowerRequestSystemRequired);
  CloseHandle(hPCR);
#endif
  // SetThreadExecutionState(ES_CONTINUOUS);
}