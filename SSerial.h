#pragma once

#include <windows.h>

extern HANDLE hCom;
extern DCB dcb;

HANDLE openSerial(const char* portName, int baudRate = 921600);
