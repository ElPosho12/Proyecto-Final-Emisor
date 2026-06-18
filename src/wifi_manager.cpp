#include "wifi_manager.h"
#include <Arduino.h>

bool   wifiConnected  = false;
String savedSSID      = "";
String savedPassword  = "";
String wifiFailReason = "";

// Carga las credenciales guardadas en flash
void wifiInit() {
  Preferences prefs;
  prefs.begin("wificfg", true);
  savedSSID     = prefs.getString("ssid",     "");
  savedPassword = prefs.getString("password", "");
  prefs.end();

  // Modo dual: Access Point (portal) + Station (conexión a tu red)
  WiFi.mode(WIFI_AP_STA);

  // Iniciar AP con nombre fijo y contraseña
  const char* AP_SSID = "Despertador-Config";
  const char* AP_PASS = "12345678";          // mínimo 8 caracteres para WPA2
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.printf("[WiFi] Portal AP en: http://%s\n", WiFi.softAPIP().toString().c_str());

  // Si ya había credenciales guardadas, intentar conectar
  if (savedSSID != "") {
    wifiConnect();
  }
}

// Intenta conectarse a la red guardada
void wifiConnect() {
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

    // Sincronizar hora NTP (UTC-3 Buenos Aires, sin horario de verano)
    configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("[WiFi] NTP configurado (UTC-3)");

  } else {
    wifiConnected = false;

    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL:
        wifiFailReason = "Red no encontrada. ¿El nombre es correcto?";
        break;
      case WL_CONNECT_FAILED:
        wifiFailReason = "Contraseña incorrecta.";
        break;
      default:
        wifiFailReason = "No se pudo conectar (código " + String(WiFi.status()) + ")";
    }
    Serial.println("[WiFi] Error: " + wifiFailReason);
  }
}

bool wifiIsConnected() {
  // Re-verificar en tiempo real por si se cayó la conexión
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  return wifiConnected;
}
