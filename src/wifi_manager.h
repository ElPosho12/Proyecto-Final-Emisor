#pragma once

#include <WiFi.h>
#include <Preferences.h>

// Estado de conexion, accesible desde otros modulos
extern bool   wifiConnected;
extern String savedSSID;
extern String savedPassword;
extern String wifiFailReason;

void iniciarWifi();
void conectarWifi();
bool wifiEstaConectado();