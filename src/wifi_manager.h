#pragma once

#include <WiFi.h>
#include <Preferences.h>

// Estado de conexion, accesible desde otros modulos
extern bool   wifiConnected;
extern String savedSSID;
extern String savedPassword;
extern String wifiFailReason;

// true cuando se guardaron credenciales nuevas desde la web (main la consume)
extern volatile bool wifiRecienReconfigurado;

void iniciarWifi();
void conectarWifi();
bool wifiEstaConectado();
void reactivarPortalWifi();
void apagarPortalWifi();