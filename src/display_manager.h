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

// Pantallas de conexión
void displayWifiConectado();
void displayBtConectado();

// Franja fija en la parte inferior de la pantalla: muestra "Buscando WiFi..."
// y/o "Buscando Bluetooth..." mientras cada uno no esté conectado, y se
// oculta apenas se encuentra. Convive con cualquier otra pantalla.
void displayEstadoBusqueda(bool wifiConectado, bool btConectado);