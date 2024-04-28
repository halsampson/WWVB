#include "sserial.h"

HANDLE hCom;
DCB dcb;

HANDLE openSerial(const char* portName, int baudRate) {
  char portDev[16] = "\\\\.\\";
  strcat_s(portDev, sizeof(portDev), portName);
  hCom = CreateFileA(portDev, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, NULL, NULL);
  if (hCom == INVALID_HANDLE_VALUE) return hCom;

  dcb.DCBlength = sizeof(DCB);
  dcb.BaudRate = baudRate;
  dcb.ByteSize = 8;
  dcb.fBinary = TRUE;
  dcb.fDtrControl = DTR_CONTROL_DISABLE;
  if (!SetCommState(hCom, &dcb)) return 0;
  if (!SetupComm(hCom, 16, 64)) return 0;

  COMMTIMEOUTS timeouts = { 0 };  // in ms
  timeouts.ReadTotalTimeoutConstant = 3000 + 265;  // for Hz + more for large capacitance (rare TODO)
  timeouts.ReadIntervalTimeout = 64;  // bulk USB 64 byte partial buffer timeout
  if (!SetCommTimeouts(hCom, &timeouts)) return 0;

  return hCom;
}
