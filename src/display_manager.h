#pragma once

#include <Arduino.h>
#include "time.h"

// ─── Funciones de pantalla ────────────────────────────────────────────────────

void displayInit();
void displayConfirmPrevious(int hour, int minute);
void displaySetHour(int hour);
void displaySetMinute(int minute);

// oximetroActivo: true = MAX30102 muestreando, false = apagado por combo +/-
void displayClock(struct tm timeinfo, int alarmHour, int alarmMinute, bool alarmEnabled, bool oximetroActivo);

void displaySetBTStatus(bool connected);
void displayAlarmFired();