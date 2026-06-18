#pragma once

#include <WiFi.h>
#include <Preferences.h>

// Estado de la conexión accesible desde otros módulos
extern bool   wifiConnected;
extern String savedSSID;
extern String savedPassword;
extern String wifiFailReason;

void wifiInit();
void wifiConnect();
bool wifiIsConnected();
