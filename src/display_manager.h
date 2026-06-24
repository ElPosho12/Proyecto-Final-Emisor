#pragma once

#include <Arduino.h>
#include "time.h"

void displayInit();
void displayConfirmPrevious(int hour, int minute);
void displaySetHour(int hour);
void displaySetMinute(int minute);

// Ahora el reloj recibe los estados de red directamente para dibujar los testigos en vivo
void displayClock(struct tm timeinfo, int alarmHour, int alarmMinute, bool alarmEnabled, bool oximetroActivo, bool wifiConectado, bool btConectado);

void displayAlarmFired();
void displayResetMenuState();