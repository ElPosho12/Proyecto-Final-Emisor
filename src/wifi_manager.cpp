#include "wifi_manager.h"
#include <Arduino.h>

bool   wifiConnected  = false;
String savedSSID      = "";
String savedPassword  = "";
String wifiFailReason = "";

static const char* AP_SSID = "Despertador-Config";
static const char* AP_PASS = "12345678";   // WPA2 pide 8 caracteres minimo

// Levanta el portal de configuracion (AP + STA).
// Si el AP ya esta activo no hace nada, para no cortar a quien este conectado.
static void levantarAP() {
  if (WiFi.getMode() & WIFI_AP) return;

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("[WiFi] Portal AP en: http://%s\n", WiFi.softAPIP().toString().c_str());
}

// Carga credenciales guardadas y conecta si hay
void iniciarWifi() {
  Preferences prefs;
  prefs.begin("wificfg", true);
  savedSSID     = prefs.getString("ssid",     "");
  savedPassword = prefs.getString("password", "");
  prefs.end();

  levantarAP();

  // Baja potencia de TX: menos interferencia con el Bluetooth
  WiFi.setTxPower(WIFI_POWER_11dBm);

  if (savedSSID != "") {
    conectarWifi();
  }
}

// Intenta conectar a la red guardada
void conectarWifi() {
  if (savedSSID == "") return;

  Serial.printf("[WiFi] Conectando a '%s' ...\n", savedSSID.c_str());
  wifiConnected  = false;
  wifiFailReason = "";

  WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("[WiFi] Conectado. IP: %s\n", WiFi.localIP().toString().c_str());

    // Hora por NTP (UTC-3 Buenos Aires)
    configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("[WiFi] NTP configurado.");

    // El AP queda encendido para poder cambiar el WiFi en cualquier momento
    levantarAP();
    Serial.printf("[WiFi] Portal AP sigue activo en: http://%s\n", WiFi.softAPIP().toString().c_str());

  } else {
    wifiConnected = false;

    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL:
        wifiFailReason = "Red no encontrada. ¿El nombre es correcto?";
        break;
      case WL_CONNECT_FAILED:
        wifiFailReason = "Contrasena incorrecta.";
        break;
      default:
        wifiFailReason = "No se pudo conectar (codigo " + String(WiFi.status()) + ")";
    }
    Serial.println("[WiFi] Error: " + wifiFailReason);

    levantarAP();
  }
}

bool wifiEstaConectado() {
  bool ahora = (WiFi.status() == WL_CONNECTED);

  // Si se cayo la conexion, avisa y se asegura de que el portal siga activo
  if (wifiConnected && !ahora) {
    Serial.println("[WiFi] Conexion perdida: el portal sigue activo.");
    levantarAP();
  }

  wifiConnected = ahora;
  return wifiConnected;
}