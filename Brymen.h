#pragma once

// Brymen BM857 DMM support

bool openBrymen();
void closeBrymen();

void requestReading();
double getReading();
double fastGetReading(); // returns 0 if not ready

extern const double MinErrVal;