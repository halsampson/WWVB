// WWVB.cpp       WWVB decoding experiments
// 
// Experiment comparing received with calculated bits to check error rates

#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <corecrt_share.h>

#include "waveIn.h"

#define AudDeviceName "Realtek High Def" 

// #define START_AT_HOUR 1 

// TODO: resync rough ms offset after recorded audio gaps: seen in magnitude offset ms reports *****
//   or Windows fix audio glitches
//   or port to Linux
// 
// TODO: auto-sync seconds: initial sync words search

// TODO: auto-adjust SamplingOffset_ms and SampleHz based on WWVB amplitude and phase offsets

// TODO: port to small MCU with precise 240 kHz ADC sampling so sin and cos values are only -1,0,+1

const int  MaxPhaseAvgCount = 8;  // TODO: adjust phase servo gain for best tracking, lock in, and noise rejection
   // gain is reduced by noise squelch
   // optimum depends on phase noise and drift (ionosphere bounce height), accuracy of SampleHz, ...

const bool AverageTimeFrames = false;   // for noisy evening signal; problem: sometimes wrong phase inverted state due to noise
const int  MaxNoiseAvgCount = 8;

const int  SamplingOffset_ms = -1000/4 + 170; // to center sample buffer window on 2nd half (want slice3StartSample ~ BufferSamples / 4)	
const int MaxOffsetAvgCount = 60 - 6 - 1; // decaying average over reporting minute

// TODO: decode Six Minute frames for 15dB better time signal
//   106 bit compare

// TODO: Try I Q (I V ?) separation into more common 48kHz stereo sampling (although many mic inputs are mono)

const int WWVBHz = 60000;

char frameType = '_'; // sync, time, extended, fixed, message
signed char frameBits[60] = { // -1: 0,  1: 1,  0: not checked
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
  char time_par[5] = {0};

  unsigned int minute = (unsigned int)(time(NULL) / 60 - 15778080); // in this century
  int bit = 0;
  for (int sec = 46; sec >= 18; --sec) {  // 26 + 3 marker rcvdBits
    if (sec % 10 == 9) continue; // marker
    frameBits[sec] = minute & 1;

    if (minute & 1)
    for (int par = 0; par < 5; ++par)
      if (parVector[par] & (1 << bit))
        time_par[par] ^= 1;
    minute >>= 1;
    ++bit;
  }

  for (int par = 0; par < 5; ++par)
    frameBits[17 - par] = time_par[par];

  const signed char dst[13] = {-1,-1, 0, -1, 1, 1,-1, 1, 1,-1, 1, 1, 0,};  // no leap second, summer DST
  memcpy(frameBits + 47, dst, sizeof dst);
}

void set106bitTimingWord(int minuteMod30) {
  // six minutes = 360 bits =     127 + 106 + 127
  const signed char FixedTimingWord[7 + 106 + 7] = {
   -1,-1,-1,-1,-1,-1,-1,
    1, 1, 0, 1, 0, 0, 0, 1, 1, 1,
    0, 1, 0, 1, 1, 0, 0, 1, 0, 1,
    1, 0, 0, 1, 1, 0, 1, 1, 1, 0,
    0, 0, 1, 1, 0, 0, 0, 0, 1, 0,
    1, 1, 0, 1, 0, 0, 1, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 0, 1, 0, 0,
    0, 0, 1, 0, 1, 1, 1, 0, 0, 0,
    1, 0, 1, 1, 0, 1, 0, 1, 1, 0,
    1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 1, 0, 0,
    1, 0, 0, 1, 0, 0,
   -1,-1,-1,-1,-1,-1,-1,
  };
  memcpy(frameBits, FixedTimingWord + (minuteMod30 == 13 ? 60 : 0), sizeof frameBits);
  frameType = 'f';
}

const double PI = 3.141592653589793238463;
const double TwoPI = 2 * PI;

double normalize(double phase) {
  return fmod(phase + TwoPI + PI, TwoPI) - PI; // -PI..PI
}

unsigned long long rcvdBits;

int second;
double avgPhaseOffset, lastCorrection;

double cleanPhaseOffsetTotal;
int cleanPhaseOffsetCount;

FILE* fLog;

void printfLog(const char* format, ...) {
  va_list args; va_start(args, format);
  static char line[256];
  static int linePos;
  int start = linePos;
  linePos += vsprintf_s(line + linePos, sizeof line - linePos, format, args);
  va_end(args);
  printf("%s", line + start);
  if (line[linePos - 1] == '\n') {
    if (!fLog) fLog = _fsopen("logAll.txt", "ab", _SH_DENYNO);
    if (fLog) fwrite(line, 1, linePos, fLog);
    linePos = 0;
    // fflush(fLog);
  }
}

void adjustPhase(double& phase, double  magOffset, double phaseDifference) {
  // reduce phase servo gain when 0.25s slices amplitudes and/or phases don't match:
  double noiseSquelch = (1 - sqrt(fabs(magOffset))) * (1 - sqrt(fabs(phaseDifference) / PI)); 

  if (AverageTimeFrames && frameType == 't' && second >= 20 && (second < 43 || second > 46)) { // avoid averaging minutes LSBs which change
    static double avgPhase[60]; // for each second in minute frame
    static int noiseAvgCount = 1;
    avgPhase[second] += normalize(phase - avgPhase[second]) / noiseAvgCount * noiseSquelch;
    phase = avgPhase[second] = normalize(avgPhase[second]); // use avg vs. noise
    if (second == 58 && noiseAvgCount < MaxNoiseAvgCount) ++noiseAvgCount; // once a minute
  }

  // ?? better PID servo to handle short and long-term drifts vs. noise?
  double bitPhase = normalize(phase - avgPhaseOffset);  // should be near 0 or +/-PI
  double phaseOfs = fmod(bitPhase + TwoPI + PI/2, PI) - PI/2;   // -PI/2..PI/2, independent of phase inversion

  static int phaseAvgCount = 1;
  avgPhaseOffset += lastCorrection = phaseOfs / phaseAvgCount * noiseSquelch;
  if (phaseAvgCount < MaxPhaseAvgCount) ++phaseAvgCount;

  if (fabs(phaseDifference) < PI / 16) { // clean phase measurement threshold
    cleanPhaseOffsetTotal += phaseOfs;  // per second;  ?biases?
    ++cleanPhaseOffsetCount;
  }

  bool phaseInverted = fabs(normalize(phase - avgPhaseOffset)) >= PI/2;
  int bit = phaseInverted ? 1 : 0; 

  int syncBit = frameBits[second];
  if ((frameType == 't' || frameType == 'f') && syncBit >= 0 && bit != syncBit) // check known bits
    printfLog("%c", bit ? 'o' : '!'); // miscompare
  else printfLog("%d", bit);

  rcvdBits <<= 1;
  rcvdBits |= bit;
}

int bitCount(unsigned long long rcvdBits) { 
  int count = 0;
  while (rcvdBits) {
    count += ((rcvdBits & 3) + 1) / 2;
    rcvdBits >>= 2;
  }
  return count;
}

static SYSTEMTIME systemTime;

void setFrameType() {
  if (frameType == 'x') {
    printfLog("__x");
    return;
  }

  if (frameType == 'f' && systemTime.wMinute % 30 == 12) {
    printfLog("__f");
    return;  
  }

  int syncBitsMatched;

  if (frameType == 'f') { // minute 13 or 43 -- fixed bits
    syncBitsMatched = 11 - 2 * bitCount(rcvdBits ^ 0b01010000011);
  } else { // can be time or message
    const int TimeSync = 0b01110110000; // 01110110m000 time sync,    inverted: 10001001m111  
    const int MsgSync  = 0b10100011010; // 10100011m010 message sync, inverted: 01011100m101

    int timeSyncBitsMatched = 11 - 2 * bitCount(rcvdBits ^ TimeSync); // inverted match -11..11 all matched
    int msgSyncBitsMatched  = 11 - 2 * bitCount(rcvdBits ^ MsgSync);
    //  rcvdBits matched is negative when phase is inverted

    // choose best match, including inverted phase -- most likely time
    frameType = abs(timeSyncBitsMatched) >= abs(msgSyncBitsMatched) ? 't' : 'm';
    syncBitsMatched = frameType == 't' ? timeSyncBitsMatched : msgSyncBitsMatched;
  }
  if (syncBitsMatched <= -7)    // sync word phase very likely inverted -- should be rare except when noisy
    avgPhaseOffset += PI; // invert phase

  printfLog("%+d%c", syncBitsMatched - syncBitsMatched / 4, frameType);  // scaled to single digit -9..9
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

double bufferStartSeconds;

FILE* fMagPh;

void processBuffer(int b) {
  static double avgMag, avgMagOfs, avgPhaseDifference;

  if (second == 0) {
    printfLog("\n");

    rcvdBits = 0;
    frameType = 's'; // sync until known

    GetSystemTime(&systemTime);
    printfLog("%2d:%02d:%02d.%03d ", systemTime.wHour, systemTime.wMinute, systemTime.wSecond, systemTime.wMilliseconds);

    long long sumSquares = 0;
    for (int s = 0; s < BufferSamples; ++s)
      sumSquares += wavInBuf[b][s] * wavInBuf[b][s];
    printfLog("%3.0fdB ", 20 * log10(avgMag) - 10 * log10((double)sumSquares)); // SNR
    printfLog("%+4.0fms %+5.1f ", avgMagOfs * 500, avgPhaseDifference * 180 / PI);  // both s/b near 0

    int minuteMod30 = systemTime.wMinute % 30;
    if (minuteMod30 == 12 || minuteMod30 == 13)
      set106bitTimingWord(minuteMod30);
    else if (minuteMod30 >= 10 && minuteMod30 <= 15)
      frameType = 'x';
    else setMinutesInCentury();
  }

  if (second % 60 == 0 || second % 10 == 9) { // marker second -- low signal
    printfLog("%c", frameType);  
    avgPhaseOffset += lastCorrection;
    return; // don't process low signal marker seconds
  }

  // process last half of bit time where amplitude is always high
  int slice3StartSample = (int)round(fmod((bufferStartSeconds + 0.5) * SampleHz, BufferSamples));  // ~ BufferSamples / 4
  int slice3EndSample = slice3StartSample + BufferSamples / 4; 

  MagPhase slice3 = processSlice(slice3StartSample, slice3EndSample, wavInBuf[b]); // bit start + 0.5..0.75s
  MagPhase slice4 = processSlice(slice3EndSample, min(slice3EndSample + BufferSamples / 4, BufferSamples), wavInBuf[b]);  // bit start + 0.75..1.0s 

  static int offsetAvgCount = 1;
  double magOffset = (slice4.mag - slice3.mag) / (slice3.mag + slice4.mag); // 0 if no noise, centered on slices
  avgMagOfs += (magOffset - avgMagOfs) / offsetAvgCount;
  // TODO: servo avgMagOfs toward 0 vs. clock drift

  avgMag += (slice3.mag + slice4.mag - avgMag) / offsetAvgCount;

  // also show deviation = noise

  double phaseDifference = normalize(slice4.ph - slice3.ph); // 0 if no short-term phase noise/drift
  avgPhaseDifference += (phaseDifference - avgPhaseDifference) / offsetAvgCount;
  avgPhaseDifference = normalize(avgPhaseDifference);
  if (offsetAvgCount < MaxOffsetAvgCount) ++ offsetAvgCount;

  if (systemTime.wYear) { // after systemTime set at first second == 1
    unsigned short hms = systemTime.wHour << 12 | systemTime.wMinute << 6 | systemTime.wSecond;  // 4 + 6 + 6 rcvdBits
    short tMagPh[5] = {(short)hms, (short)(slice3.mag / 100), (short)(slice3.ph * 180 / PI), 
                                   (short)(slice4.mag / 100), (short)(slice4.ph * 180 / PI)};
    fwrite(tMagPh, sizeof tMagPh, 1, fMagPh);
  }
  
  double q3PhaseOfs = normalize(TwoPI * WWVBHz * (bufferStartSeconds + slice3StartSample / SampleHz));
  double phase = normalize((slice3.ph + slice4.ph) / 2 - q3PhaseOfs);

  adjustPhase(phase, magOffset, phaseDifference); 

  if (second == 12 && systemTime.wYear) // sync done
    setFrameType();

  avgPhaseOffset = normalize(avgPhaseOffset);
}

void audioReadyCallback(int b, int samplesRecorded) {
  second = int(fmod(bufferStartSeconds + 0.5, 60));
  if (samplesRecorded == BufferSamples)
    processBuffer(b);
  else {
    printf("b");
    avgPhaseOffset += lastCorrection;
  }
  bufferStartSeconds += samplesRecorded / SampleHz; // next buffer start time
  // TODO: samples might be discontinuous -- check for sudden jump in avgMagOffset and resync
}

double clockOffSeconds;

void alignOutput() {
  printf("\n UTC");
  for (int i= 0; i < 32 + int(bufferStartSeconds) % 60; ++i) printf(" ");
}

void startAudioIn() {
  extern double ntpTime();
  double ntp = ntpTime();
  SYSTEMTIME nearNTP_time; GetSystemTime(&nearNTP_time);

  int ms = (int)(fmod(ntp, 1) * 1000);
  int sleep_ms = 2000 - ms + SamplingOffset_ms;
  Sleep(sleep_ms);   // start near 1 second boundary

  startWaveIn();  // for consistent phase
  SYSTEMTIME wavInStartTime; GetSystemTime(&wavInStartTime);

  double stNtp = nearNTP_time.wSecond + nearNTP_time.wMilliseconds / 1000.;
  double stWis = wavInStartTime.wSecond + wavInStartTime.wMilliseconds / 1000.;
  bufferStartSeconds = fmod(ntp + stWis - stNtp, 60); // approx recording start time

  // check also with https://nist.time.gov/  Your clock is off by: 
  clockOffSeconds = fmod(stNtp - fmod(ntp, 60) + 60 + 30,  60) - 30;  // -: CPU behind
  printf("%.3fs   (PC clock off %+.3fs)", bufferStartSeconds, clockOffSeconds); 
  alignOutput();
}

int main() {


#ifdef START_AT_HOUR // PDT: delayed clean night higher SNR start
  const int SecsPerHour = 60 * 60;
  const int SecsPerDay = 24 * 60 * 60;
  int secsUntilStart = ((START_AT_HOUR + 7) * SecsPerHour - time(NULL) % SecsPerDay + SecsPerDay) % SecsPerDay;
  Sleep(1000 * secsUntilStart);
#endif

  fMagPh = _fsopen("magPhs.bin", "wb", _SH_DENYNO); 

  setupAudioIn(AudDeviceName, &audioReadyCallback);
  startAudioIn();
  
  while (1) {
    switch(_getch()) {
      case 'f' : if (1 || cleanPhaseOffsetCount > 1000) {
        double avgPhaseOffsetPerSec = cleanPhaseOffsetTotal / cleanPhaseOffsetCount;
        double avgCyclesPerSecOffset = avgPhaseOffsetPerSec / TwoPI;   // cycles of WWVBHz per second
        printf("\nSampleHz off by: %+.4f Hz", avgCyclesPerSecOffset * SampleHz / WWVBHz);  // TODO: check calc by changing SampleHz ***
        alignOutput();
        }
        break;
      case 's' : // stop
        printf("\nstop\n");
        if (fMagPh) fclose(fMagPh);
        if (fLog) fclose(fLog);
        stopWaveIn();
        exit(0);
    }
  }

  return 0;
}

