#pragma once

#include <Arduino.h>
#include "time.h"

// Variables compartidas para el control de refresco
extern int lastMinute;

void displayInit();
void displayConfirmPrevious(int hour, int minute);
void displaySetHour(int hour);
void displaySetMinute(int hour, int minute);
void displayClock(struct tm timeinfo, int alarmHour, int alarmMinute, bool alarmEnabled, bool oximetroActivo, bool wifiConectado, bool btConectado);
void displayAlarmFired();
void displayResetMenuState();