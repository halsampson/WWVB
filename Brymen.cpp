#include <windows.h>
#include <stdio.h>
#include "Brymen.h"
#include "sserial.h"

// Brymen BM857 DMM support

const char* comPort = "COM31"; // Fixed by serial number; Tx = DTR
const int Brymen857Baud = 1000000 / 128;  // 128us per bit

const int RawLen = 35; 

const double MinErrVal = 1E9;

typedef struct {
  BYTE start : 2;  // always 1
  BYTE data  : 4;
  BYTE stop  : 2;  // always 3
} rawData;

rawData raw[RawLen];

// TODO: fill in unknown LCD bits

struct {
  union {
    BYTE switchPos;
    struct {
      BYTE swPosLSN : 4;
      BYTE swPosMSN : 4;
    };
  };

  BYTE dBm   : 1;
  BYTE milli : 1;
  BYTE micro : 1;
  BYTE Volts : 1;
  BYTE Hz    : 1;
  BYTE Ohms  : 1;
  BYTE kilo  : 1;
  BYTE Mega  : 1;

  BYTE Auto : 1;
  BYTE Hold : 1;
  BYTE DC : 1;
  BYTE AC : 1;
  BYTE unk7 : 2; // V; A?
  BYTE nano : 1;
  BYTE unk8 : 1; // FS?

  struct {
    BYTE segments  : 7;
    BYTE dpOrMinus : 1;
  } lcdDigits[6];

  BYTE bar0to5 : 6;
  BYTE beep : 1;
  BYTE unk1 : 1;  // LowBatt?

  BYTE bargraph[5]; // 6 to 45

  BYTE unk2 : 1; 
  BYTE Min  : 1;
  BYTE unk3 : 1;  // Avg ?
  BYTE percent : 1;
  BYTE bar46to49 : 4;

  BYTE unk4 : 4;
  BYTE PeaktoPeak : 1; // ??
  BYTE unk5 : 1; 
  BYTE Max : 1;
  BYTE Delta : 1;
  // Crest ?

  BYTE unk6 : 8;
} packed;


bool packRaw(void) {
  for (int b = 0; b < RawLen; b++)
    if (raw[b].start != 1 || raw[b].stop != 3) {
      printf("Err %d ", b);  // only 4 bits vary
      return false;
    }

  BYTE* p = (BYTE*)&packed;
  rawData* pRaw = raw + 1;
  do { // pack raw nibbles into packed bytes
    *p = (*pRaw++).data << 4;
    *p++ |= (*pRaw++).data;
  } while (pRaw < raw + sizeof(raw));

  packed.switchPos = ~packed.switchPos;  // single bit sent inverted
  return true;
}


char numStr[8];
const char* range;

double getLcdValue(void) {
  const char LCDchars[] = "0123456789ELnr-";
  const char Segments[] = { // 7 segment bits
     0x7D,    5, 0x5B, 0x1F, 0x27, // 0..4
     0x3E, 0x7E, 0x15, 0x7F, 0x3F, // 5..9
     0x7A, 0x68, 0x46, 0x42, 2,    // ELnr-
     0
  };

  char* pNum = numStr;
  for (int digit = 0; digit < sizeof(packed.lcdDigits); digit++) {
    if (packed.lcdDigits[digit].dpOrMinus) *pNum++ = digit ? '.' : '-';
    if (packed.lcdDigits[digit].segments) { // not blank
      const char* lcdChar = strchr(Segments, (char)(packed.lcdDigits[digit].segments));
      if (lcdChar) *pNum++ = LCDchars[lcdChar - Segments];
    }
  }
  *pNum = 0;
  double value = atof(numStr);

  range = "";
  if (!packed.dBm && !packed.percent) {
    if (packed.nano)  { range = "n"; value *= 1E-9; }
    if (packed.micro) { range = "u"; value *= 1E-6; }
    if (packed.milli) { range = "m"; value *= 1E-3; }
    if (packed.kilo)  { range = "k"; value *= 1E3; }
    if (packed.Mega)  { range = "M"; value *= 1E6; }
  }
  return value;
}

const char* units;
const char* acdc;
const char* modifier;

void getUnits(void) {  // sets 3 strings above
  const char* unitStr[] = { "V", "V", "V", "Hz", "V", "Ohm", "F", "A", "A", "dBm", "%" };
  BYTE switchPos = 0;
  if (packed.switchPos) {
    const int log2[9] = { 0, 1, 2, 2, 3, 3, 3, 3, 4 }; // bit position
    if (packed.swPosMSN)
      switchPos = 5 - log2[packed.swPosMSN];
    else switchPos = 4 + log2[packed.swPosLSN];
  }
  if (packed.dBm) switchPos = 9; // dBm  
  else if (packed.percent) switchPos = 10; // %

  units = unitStr[switchPos];

  acdc = "";
  if (packed.AC && packed.DC) {
    acdc = " AC+DC";
  } else {
    if (packed.DC) acdc = "DC";
    if (packed.AC) acdc = "AC";
  }

  modifier = "";
  if (packed.Min) modifier = "Min";
  if (packed.Max) modifier = "Max";
  if (packed.PeaktoPeak) modifier = "p-p"; // ?not seen
  if (packed.Delta) modifier = "Delta"; 
  // if (packed.Rec) modifier = "Rec";
  
  // unknown LCD bits:
  // printf("%X %X%X%X%X %X%X%X%X ", raw[0].data, packed.unk1, packed.unk2, packed.unk3, packed.unk4, packed.unk5, packed.unk6, packed.unk7, packed.unk8);
}

double decodeRaw(bool doUnits = true) {
  if (!packRaw()) return MinErrVal;

  double reading = getLcdValue();
  if (doUnits) getUnits();
  return reading;
}

HANDLE hBrymen;

void requestReading() {
  dcb.fDtrControl = DTR_CONTROL_ENABLE; // low -> IRED on
  SetCommState(hBrymen, &dcb);
}

double getReading() {
  requestReading();

  DWORD bytesRead = 0;
  ReadFile(hBrymen, raw, RawLen, &bytesRead, NULL);
  
  dcb.fDtrControl = DTR_CONTROL_DISABLE;
  SetCommState(hBrymen, &dcb);
  
  if (bytesRead != RawLen) return MinErrVal;
  
  return decodeRaw();
}

double fastGetReading() {
  if (rxRdy() >= RawLen)
    return getReading();

  dcb.fDtrControl = DTR_CONTROL_DISABLE;
  SetCommState(hBrymen, &dcb);
  return MinErrVal;
}

bool openBrymen() {
  if ((hBrymen = openSerial(comPort, Brymen857Baud)) <= (HANDLE)0) {
    printf("Connect Brymen to special %s\n", comPort);
    return false;
  }
  bool brymenOK = getReading() < MinErrVal;
  if (brymenOK)
    printf("Brymen connected on %s\n", comPort);

  return brymenOK;
}

void closeBrymen() {
  if (hBrymen)
    CloseHandle(hBrymen);
}