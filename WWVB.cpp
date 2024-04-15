// WWVB.cpp    WWVB decoding

#define AudDeviceName "Realtek High Def" 
extern const double SampleHz = 191996.73;    // Audio sampling rate measured by recording 1PPS for 100s

// Now: experimental comparison with predicted bits 
// TODO: auto-sync to WWVB sync word
// TODO: auto-adjust MagnitudeOffset_ms SampleHz based on WWVB amplitude and phase measurments

const int MagnitudeOffset_ms = -60; // as reported in column 4 of output

const int MaxAvgCount = 8; // adjust servo gain for best lock vs. noise rejection

#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <corecrt_share.h>

const int WWVBHz = 60000;

int sync[60] = { // -1: 0,  1: 1,  0: not checked
// 0, 1,-1, 1,-1,-1,-1, 1, 1, 0, -1, 1,-1,  // Message frame sync
   0,-1, 1, 1, 1,-1, 1, 1,-1, 0, -1,-1,-1, 0, 0, 0, 0, 0, 0, 0, // Time frame sync
  -1, 1, 1,-1,-1,-1,-1, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // minutes: filled in by setMinutesInCentury()
   0, 0, 0, 0, 0, 0, 0,-1,-1, 0, -1, 1, 1,-1, 1, 1,-1, 1, 1, 0,
}; 

// 0 011 1011 0m000 time sync
// 1 101 0001 1m010 message sync 

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

const double PI = 3.141592653589793238463;
const double TwoPI = 2 * PI;

double normalize(double phase) {
  return fmod(phase + TwoPI + PI, TwoPI) - PI; // -PI..PI
}

unsigned long long bits;
bool timeFrame; // else message frame

int second;
double avgPhaseOffset;

void checkPhase(double& phase, double  magOffset) {
  double noiseSquelch = 1 - fabs(magOffset);  // reduce weight in case of mismatched amplitudes (up to +/- 1)

  static double avgPhase[60];
  static int avgCount = 1;
  avgPhase[second] += normalize(phase - avgPhase[second]) / avgCount * noiseSquelch;
  if (avgCount < MaxAvgCount) ++avgCount;
  if (0) phase = avgPhase[second] = normalize(avgPhase[second]); // use avg vs. noise

  double bitPhase = normalize(phase - avgPhaseOffset);  // should be near 0 or +/-PI
  double phaseOfs = fmod(bitPhase + PI/2, PI) - PI/2; // independent of phase reversal
  static int syncAvgCount = 1;
  avgPhaseOffset += phaseOfs / syncAvgCount * noiseSquelch;
  if (syncAvgCount < MaxAvgCount) ++syncAvgCount;

  bool phaseInverted = fabs(normalize(phase - avgPhaseOffset)) > PI / 2;
  int bit = phaseInverted ? 1 : 0; 
  int syncBit = sync[second];
  if (timeFrame && syncBit && bit != syncBit > 0)
    printf("%c", bit ? 'o' : '!'); // miscompare
  else printf("%d", bit);

  bits <<= 1;
  bits |= bit;
}

int bitCount(unsigned long long bits) { 
  int count = 0;
  while (bits) {
    count += ((bits & 3) + 1) / 2;
    bits >>= 2;
  }
  return count;
}

static SYSTEMTIME st;

void checkSync() {
  const int TimeSync = 0x3B0; // 01110110m000 time sync,    inverted: 10001001m111  
  const int MsgSync  = 0x51A; // 10100011m010 message sync, inverted: 01011100m101

  int timeSyncCount = 11 - 2 * bitCount(bits ^ TimeSync); // -11..11
  int msgSyncCount  = 11 - 2 * bitCount(bits ^ MsgSync);
  // get negative bit match counts when phase is inverted

  // ?? seeing other sync words?  messages at minutes 10..15, 40..45
  bool messageLikely = st.wMinute % 30 >= 10 && st.wMinute % 30 <= 15;
  // choose best match count, including inverted phase; prefer message display when unsure
  timeFrame = abs(timeSyncCount) > abs(msgSyncCount) + (messageLikely ? 6 : 1);
  int syncCount = timeFrame ? timeSyncCount : msgSyncCount;
  printf(syncCount >= 0 ? "+" : "-");
  printf("%d", max(abs(syncCount) - 2, 0));  // confidence level <= 9
  printf(timeFrame ? "t" : "m");

  if (syncCount < -5)     // phase very likely inverted 
    avgPhaseOffset += PI; // flip phase to match sync word
}

typedef struct {
  double mag, ph;
} MagPhase;

MagPhase processSlice(int startSample, int endSample, short* buf) {
  double i = 0, q = 0;
  for (int s = startSample; s < endSample; ++s)  {  
    double theta = TwoPI * WWVBHz / SampleHz;
    i += cos(s * theta) * buf[s];
    q += sin(s * theta) * buf[s];
  }
  return MagPhase {sqrt(pow(i, 2) + pow(q, 2)) / 48, atan2(i, q)}; // -PI..PI
}

extern const int BufferSamples = int(SampleHz + 0.5);
extern short wavInBuf[2][BufferSamples];  
double seconds;

double clockOff;  // automatic using NTP; check with https://nist.time.gov/  Your clock is off by: 

FILE* fMagPh;

void processBuffer(int b) {
  MagPhase q3 = processSlice(BufferSamples / 2, BufferSamples * 3 / 4, wavInBuf[b]);
  MagPhase q4 = processSlice(BufferSamples * 3 / 4, BufferSamples, wavInBuf[b]); 

  static double avgMagOfs, avgPhaseOfs;
  static int avgCount = 1;
  double magOffset =(q4.mag - q3.mag) / (q3.mag + q4.mag);
  avgMagOfs += (magOffset - avgMagOfs) / avgCount;
  avgPhaseOfs += (normalize(q4.ph - q3.ph) - avgPhaseOfs) / avgCount;
  avgPhaseOfs = normalize(avgPhaseOfs);

  // TODO: servo avgMagOfs toward 0 vs. clock drift

  if (second == 1) {
    printf("\n");

    bits = 0;
    timeFrame = false;

    GetSystemTime(&st);
    printf("%2d ", st.wMinute);

    long long sumSq = 0;
    for (int s = 0; s < BufferSamples; ++s)
      sumSq += wavInBuf[b][s] * wavInBuf[b][s];
    printf("%3.0fdB ", 20 * log10(q3.mag + q4.mag) - 10 * log10((double)sumSq)); // SNR

    double sec = st.wSecond + st.wMilliseconds / 1000.;
    printf("%.2f", sec - clockOff); 
    printf("%+4.0fms %+5.2f ", avgMagOfs * 500, avgPhaseOfs);  // both s/b near 0

    setMinutesInCentury();
  }
  
  double q3Phase = normalize(TwoPI * WWVBHz * (seconds + 0.5));
  double phase = normalize((q3.ph + q4.ph) / 2 - q3Phase);

  if (st.wYear) {
    unsigned short hms = st.wHour << 12 | st.wMinute << 6 | st.wSecond;  // 4 + 6 + 6 bits
    short tmagPh[3] = {(short)hms, (short)((q3.mag + q4.mag) / 100), (short)(phase * 180 / PI)};
    fwrite(tmagPh, sizeof tmagPh, 1, fMagPh);
  }

  checkPhase(phase, magOffset); 

  if (second == 12 && st.wYear) 
    checkSync();

  avgPhaseOffset = normalize(avgPhaseOffset);
}

void audioReadyCallback(int b) {   
  second = int(round(fmod(seconds, 60)));
  if (second % 60 != 0 && second % 10 != 9) 
    processBuffer(b);
  else printf(timeFrame ? "t" : "m");  // else marker second
  seconds += BufferSamples / SampleHz;
}

void startAudioIn() {
  extern double ntpTime();
  double ntp = ntpTime();
  SYSTEMTIME st; GetSystemTime(&st);

  int ms = (int)(fmod(ntp, 1) * 1000);
  int sleep_ms = 1000 - ms + MagnitudeOffset_ms;
  Sleep(sleep_ms);   // start on 1 second boundary

  extern void startWaveIn();
  startWaveIn();  // for consistent phase
  SYSTEMTIME wis; GetSystemTime(&wis);

  double stNtp = st.wSecond + st.wMilliseconds / 1000.;
  double stWis = wis.wSecond + wis.wMilliseconds / 1000.;
  seconds = fmod(ntp + stWis - stNtp, 60);

  clockOff = stNtp  - fmod(ntp, 60);  // -: CPU behind
  printf("%+7.3fs off %7.3fs %7s", clockOff, seconds, ""); 
  for (int i= 0; i < int(seconds); ++i) printf(" ");
}

int main() {
  fMagPh = _fsopen("magPh.bin", "wb", _SH_DENYNO);

  extern void setupAudioIn(const char* deviceName, void (*)(int b));
  setupAudioIn(AudDeviceName, &audioReadyCallback);

  startAudioIn();
  
  while (1) {
    // see also MM_WOM_DONE: https://learn.microsoft.com/en-us/windows/win32/multimedia/processing-the-mm-wom-done-message
    extern bool waveInReady();
    while (!waveInReady()) {
      SYSTEMTIME st; GetSystemTime(&st);
      Sleep(500 - (st.wMilliseconds - (int)(clockOff * 1000)) / 2);
    }

    if (_kbhit()) switch(_getch()) {
      case 's' :
        printf("\nstop\n");
        fclose(fMagPh);
        extern void stopWaveIn();
        stopWaveIn();
        exit(0);
    }
  }

  return 0;
}

