#pragma once

#include <Arduino.h>
#include "time.h"

// ─── Funciones de pantalla ────────────────────────────────────────────────────

// Inicializa la pantalla TFT
void displayInit();

// Pantalla: ¿Usar alarma anterior?
void displayConfirmPrevious(int hour, int minute);

// Pantalla: Configurar hora
void displaySetHour(int hour);

// Pantalla: Configurar minutos
void displaySetMinute(int minute);

// Pantalla: Reloj principal con estado BT y alarma
void displayClock(struct tm timeinfo, int alarmHour, int alarmMinute, bool alarmEnabled);

// Actualiza solo el estado Bluetooth en la pantalla
void displaySetBTStatus(bool connected);

// Pantalla: ¡Alarma disparada!
void displayAlarmFired();
