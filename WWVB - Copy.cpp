// WWVB.cpp    WWVB decoding

// TODO:
//   extract audio, NTP classes

#define AudDeviceName "Realtek High Def" 
const double SampleHz = 191996.73;    // Audio sampling rate measured by recording 1PPS for 100s

// https://nist.time.gov/  Your clock is off by: 
double clockOff;  // automatic using NTP

const int MagOfs_ms = -40;

const int MaxAvgCount = 8; // adjust servo gain for best lock vs. noise rejection

#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <math.h>
#include <mmsystem.h>
#include <time.h>
#include <corecrt_share.h>
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib,"Ws2_32.lib")

typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

#define NTP_TIMESTAMP_70_YEARS 2208988800ull

// NTP packet struct
typedef struct {
    uint8_t li_vn_mode;      // li.   Two bits.   Leap indicator.
                             // vn.   Three bits. Version number of the protocol.
                             // mode. Three bits. Client will pick mode 3 for client.

    uint8_t stratum;         // Stratum level of the local clock.
    uint8_t poll;            // Maximum interval between successive messages.
    uint8_t precision;       // Precision of the local clock.

    uint32_t rootDelay;      // Total round trip delay time.
    uint32_t rootDispersion; // Max error allowed from primary clock source.
    uint32_t refId;          // Reference clock identifier.

    uint32_t refTm_s;        // Reference time-stamp seconds.
    uint32_t refTm_f;        // Reference time-stamp fraction of a second.

    uint32_t origTm_s;       // Originate time-stamp seconds.
    uint32_t origTm_f;       // Originate time-stamp fraction of a second.

    uint32_t rxTm_s;         // Received time-stamp seconds.
    uint32_t rxTm_f;         // Received time-stamp fraction of a second.

    uint32_t txTm_s;         // Transmit time-stamp seconds.
    uint32_t txTm_f;         // Transmit time-stamp fraction of a second.
} ntp_packet;                // Total: 48 bytes.

void err(const char *message) {
  printf("Error: %s %d\n", message, errno);
  exit(1);
}

double ntpTime() {
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData)) { printf("WSAStartup fail"); system("pause"); }

  ntp_packet ntp_p = {0};
  ntp_p.li_vn_mode = 0x1b; // Set vn  3, mode  3 

  SOCKET socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_fd < 0)
      err("open socket");

  struct hostent *ip = gethostbyname("us.pool.ntp.org");
  if (ip == NULL)
      err("hostname");

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  memcpy(&addr.sin_addr.s_addr, ip->h_addr_list[0], ip->h_length);
  addr.sin_port = htons(123); // UDP

  if (connect(socket_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
      err("connect");

  uint32_t t4 = (uint32_t)time(NULL) + NTP_TIMESTAMP_70_YEARS;
  ntp_p.txTm_s = ntohl(t4);
  if (send(socket_fd, (char *) &ntp_p, sizeof(ntp_packet), 0) < 0)
      err("write to socket");

  if (recv(socket_fd, (char *) &ntp_p, sizeof(ntp_packet), 0) < 0)
      err("read socket");

  ntp_p.txTm_s = htonl(ntp_p.txTm_s); // Seconds
  ntp_p.txTm_f = htonl(ntp_p.txTm_f);   // Fraction

/*
  FILETIME ft;
  SystemTimeToFileTime(); // 100 ns???

  SYSTEMTIME st;
  GetSystemTime(&st); // millisec

  t0 is the client's timestamp of the request packet transmission,
  t1 is the server's timestamp of the request packet reception 
  t2 is the server's timestamp of the response packet transmission and
  t3 is the client's timestamp of the response packet reception
  Time Correction = ((t2+t1)/2) - ((t3+t0)/2)

  ntp_p.refTm_s = ntohl( ntp_p.refTm_s ); // Time-stamp seconds.
  ntp_p.refTm_f = ntohl( ntp_p.refTm_f ); // Time-stamp fraction of a second.
  ntp_p.origTm_s = ntohl( ntp_p.origTm_s ); // Time-stamp seconds.
  ntp_p.origTm_f = ntohl( ntp_p.origTm_f ); // Time-stamp fraction of a second.
  ntp_p.rxTm_s = ntohl( ntp_p.rxTm_s ); // Time-stamp seconds.
  ntp_p.rxTm_f = ntohl( ntp_p.rxTm_f ); // Time-stamp fraction of a second.

  int delta = (int)(((ntp_p.rxTm_s - ntp_p.origTm_s) + (ntp_p.txTm_s - t4)) / 2);
  int delta_f = (int)(ntp_p.rxTm_f - ntp_p.origTm_f);
*/
 
  return ntp_p.txTm_s - NTP_TIMESTAMP_70_YEARS + (double)ntp_p.txTm_f / (1LL << 32);  // TODO: correct for trip delay
}

const unsigned short MicLevel = 65535;
// TODO: also set Mic Boost
//    MIXERCONTROL_CONTROLTYPE_DECIBELS ??

const double PI = 3.141592653589793238463;
const double TwoPI = 2 * PI;

const int WWVBHz = 60000;

#define WAV_IN_BUF_MSECS 1000
#define WAV_IN_CHANNELS 1
#define BITS_PER_SAMPLE 16
#define WAV_IN_SAMPLE_HZ 192000  // requested 

const int BufferSamples = int(SampleHz + 0.5);
short wavInBuf[2][BufferSamples * WAV_IN_CHANNELS];   

double seconds;
int second;

typedef struct {
  double I, Q;
} IQ;

typedef struct {
  double mag, ph;
} MagPhase;

MagPhase processSlice(int startSample, int endSample, short* buf) {
  IQ iq = {0};
  
  for (int s = startSample; s < endSample; ++s)  {  
    double theta = TwoPI * WWVBHz / SampleHz;
    iq.I += cos(s * theta) * buf[s];
    iq.Q += sin(s * theta) * buf[s];
  }

  return MagPhase {sqrt(pow(iq.I, 2) + pow(iq.Q, 2)) / 48, atan2(iq.I, iq.Q)}; // -PI..PI
}

double norm(double phase) {
  return fmod(phase + TwoPI + PI, TwoPI) - PI; // -PI..PI
}

int bitCount(unsigned int bits) {
  int count = 0;
  while (bits) {
    count += bits & 1;
    bits >>= 1;
  }
  return count;
}

FILE* fMagPh;

int sync[60] = {
#ifdef MESSAGE_FRAME
   0, 1,-1, 1,-1,-1,-1, 1, 1, 0, -1, 1,-1,
#else
   0,-1, 1, 1, 1,-1, 1, 1,-1, 0, -1,-1,-1, 0, 0, 0, 0, 0, 0, 0,
  -1, 1, 1,-1,-1,-1,-1, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // minutes: filled in below
   0, 0, 0, 0, 0, 0, 0,-1,-1, 0, -1, 1, 1,-1, 1, 1,-1, 1, 1, 0,
 #endif
}; 

// 0 011 1011 0m000 time sync
// 1 101 0001 1m010 message sync 
//     1  0 1   0 0 same 
//     3  5 7  10 12

void setMinutesInCentury() {
  /*
  time_par[0] = sum(modulo 2){time[       23,   21, 20,       17,16, 15,14,13,           9, 8,     6, 5, 4,     2,   0]}
  time_par[1] = sum(modulo 2){time[   24,    22,21,        18,17,16, 15,14,          10, 9,     7, 6, 5,     3,    1  ]}
  time_par[2] = sum(modulo 2){time[25,    23,22,        19,18,17,16, 15,          11,10,    8,  7, 6,    4,     2     ]}
  time_par[3] = sum(modulo 2){time[   24,       21,     19,18,       15,14,13,12, 11,           7, 6,    4,  3, 2,   0]}
  time_par[4] = sum(modulo 2){time[25,       22,   20,  19,      16, 15,14,13,12,           8,  7,    5, 4,  3,    1  ]
  */

  int parVector[5] = {0xB3E375, 0x167C6EA, 0x2CF8DD4, 0x12CF8DD, 0x259F1BA};  

  bool time_par[5] = {0};

  unsigned int minute = (unsigned int)(time(NULL) / 60  - 15778080); // in this century
  int bit = 0;
  for (int sec = 46; sec >= 18; --sec) {  // 26 + 3 marker bits
    if (sec % 10 == 9) continue; // marker
    sync[sec] = minute & 1 ? 1 : -1;

    if (minute & 1)
    for (int par = 0; par < 5; ++par)
      if (parVector[par] & (1 << bit))
        time_par[par] ^= 1;
    minute >>= 1;
    ++bit;
  }

  for (int par = 0; par < 5; ++par)
    sync[17 - par] = time_par[par] ? 1 : -1;
}

double avgSyncPhase;
int bits;
bool timeFrame; // else message frame

void checkPhase(double& phase) {
  static double avgPhase[60];
  static int avgCount = 1;
  avgPhase[second] += norm(phase - avgPhase[second]) / avgCount;
  if (avgCount < MaxAvgCount) ++avgCount;
  if (0) phase = avgPhase[second] = norm(avgPhase[second]); // use avg vs. noise

  double bitPhase = norm(phase - avgSyncPhase);  // should be near 0 or +/-PI
  double phaseOfs = fmod(bitPhase + PI/2, PI) - PI/2; // independent of phase reversal
  static int syncAvgCount = 1;
  avgSyncPhase += phaseOfs / syncAvgCount;
  if (syncAvgCount < MaxAvgCount) ++syncAvgCount;

  bool phaseInverted = fabs(norm(phase - avgSyncPhase)) > PI / 2;
  int bit = phaseInverted ? 1 : 0; 
  int syncBit = sync[second];
  if (timeFrame && syncBit && bit != syncBit > 0)
    printf("%c", bit ? 'o' : '!'); // miscompare
  else printf("%d", bit);

  bits <<= 1;
  bits |= bit;
}

void checkSync() {
  const int TimeSync = 0x3B0; // 01110110m000 time sync
  const int MsgSync  = 0x51A; // 10100011m010 message sync 

  int timeSyncCount = 11 - 2 * bitCount(bits ^ TimeSync); // -11..11
  int msgSyncCount  = 11 - 2 * bitCount(bits ^ MsgSync);
  timeFrame = abs(timeSyncCount) >= abs(msgSyncCount); // highest confidence
  int syncCount = timeFrame ? timeSyncCount : msgSyncCount;
  if (syncCount < 0) {  // phase likely inverted
    avgSyncPhase += PI; // flip phase to match sync word
    printf("-"); // should happen only at start or when noisy
    syncCount = -syncCount;
  } 
  printf("%d", syncCount - 2);  // confidence level <= 9
  printf(timeFrame ? "t" : "m");
}

void processBuffer(int b) {
  // TODO: wait for quiet, stable phase vs. evening power line ~60Hz harmonics > 20 dB noise
  //  else average over multiple minutes

  MagPhase q3 = processSlice(BufferSamples / 2, BufferSamples * 3 / 4, wavInBuf[b]);
  MagPhase q4 = processSlice(BufferSamples * 3 / 4, BufferSamples, wavInBuf[b]); 

  static double avgMagOfs, avgPhaseOfs;
  static int avgCount = 1;
  avgMagOfs += ((q4.mag - q3.mag) / (q3.mag + q4.mag) - avgMagOfs) / avgCount;
  avgPhaseOfs += (norm(q4.ph - q3.ph) - avgPhaseOfs) / avgCount;
  avgPhaseOfs = norm(avgPhaseOfs);

  // TODO: servo avgMagOfs toward 0  (or manually add ms to msOff for now)

  static SYSTEMTIME st;
  if (second == 1) {
    printf("\n");

    bits = 0;
    timeFrame = false;

    long long sumSq = 0;
    for (int s = 0; s < BufferSamples; ++s)
      sumSq += wavInBuf[b][s] * wavInBuf[b][s];
    printf("%.0fdB ", 20 * log10(q3.mag + q4.mag) - 10 * log10((double)sumSq)); // SNR

    GetSystemTime(&st);
    double sec = st.wSecond + st.wMilliseconds / 1000.;
    printf("%.2f", sec - clockOff); 
    printf("%+4.0fms %+5.2f ", avgMagOfs * 500, avgPhaseOfs);  // both s/b near 0

    setMinutesInCentury();
  }
  
  double q3Phase = norm(TwoPI * WWVBHz * (seconds + 0.5));
  double phase = norm((q3.ph + q4.ph) / 2 - q3Phase);

  if (st.wYear) {
    unsigned short hms = st.wHour << 12 | st.wMinute << 6 | st.wSecond;  // 4 + 6 + 6 bits
    short tmagPh[3] = {(short)hms, (short)((q3.mag + q4.mag) / 100), (short)(phase * 180 / PI)};
    fwrite(tmagPh, sizeof tmagPh, 1, fMagPh);
  }

  checkPhase(phase); 

  if (second == 12 && st.wYear) 
    checkSync();

  avgSyncPhase = norm(avgSyncPhase);
}

HWAVEIN hwi;
WAVEHDR wih[2]; 

bool waveInReady() {
  bool waveInReady = false;
   for (int b = 0; b < 2; ++b) {
    if (wih[b].dwFlags & WHDR_DONE || !wih[b].dwFlags) {
      if (wih[b].dwFlags & WHDR_DONE) {  
        second = int(round(fmod(seconds, 60)));
        if (second % 60 != 0 && second % 10 != 9) 
          processBuffer(b);
        else printf(timeFrame ? "t" : "m");  // else marker second
        seconds += BufferSamples / SampleHz;
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

void startAudioIn(const char* deviceName) {
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

  double ntp = ntpTime();
  SYSTEMTIME st; GetSystemTime(&st);

  int ms = (int)(fmod(ntp, 1) * 1000);
  int sleep_ms = 1000 - ms + MagOfs_ms;
  Sleep(sleep_ms);   // start on 1 second boundary

  res = waveInStart(hwi); // for consistent phase
  SYSTEMTIME wis; GetSystemTime(&wis);

  double stNtp = st.wSecond + st.wMilliseconds / 1000.;
  double stWis = wis.wSecond + wis.wMilliseconds / 1000.;
  seconds = fmod(ntp + stWis - stNtp, 60);

  clockOff = stNtp  - fmod(ntp, 60);  // -: CPU behind
  printf("%+7.3fs off %7.3fs %3s", clockOff, seconds, ""); 
  for (int i= 0; i < int(seconds); ++i) printf(" ");
}

int main() {
  fMagPh = _fsopen("magPh.bin", "wb", _SH_DENYNO);

  startAudioIn(AudDeviceName);
  
  while (1) {
    // see also MM_WOM_DONE: https://learn.microsoft.com/en-us/windows/win32/multimedia/processing-the-mm-wom-done-message

    while (!waveInReady()) {
      SYSTEMTIME st; GetSystemTime(&st);
      Sleep(500 - (st.wMilliseconds - (int)(clockOff * 1000)) / 2);
    }

    if (_kbhit()) switch(_getch()) {
      case 's' :
        printf("stop");
        fclose(fMagPh);
        waveInStop(hwi);
        exit(0);
    }
  }

  waveInStop(hwi);
  
  return 0;
}

