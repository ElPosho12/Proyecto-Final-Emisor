#pragma once

#include <Arduino.h>
#include "time.h"

// Variable compartida para el control de refresco
extern int UltimoMinuto;

void iniciarPantalla();
void mostrarConfirmarAnterior(int hour, int minute);
void mostrarConfigurarHora(int hour);
void mostrarConfigurarMinutos(int hour, int minute);
void mostrarReloj(struct tm timeinfo, int alarmHour, int alarmMinute, bool alarmEnabled, bool oximetroActivo, bool wifiConectado, bool btConectado);
void mostrarAlarmaSonando();
void reiniciarEstadoMenu();

// Pantallas de conexión
void mostrarWifiConectado();
void mostrarBtConectado();

// Instrucciones iniciales de WiFi (solo si no hay SSID guardado)
void mostrarInstruccionesWifi();

// Franja inferior: "Buscando WiFi..." / "Buscando Bluetooth..."
void mostrarEstadoBusqueda(bool wifiConectado, bool btConectado);