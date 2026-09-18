#include "wifi_manager.h"
#include <Arduino.h>

bool   wifiConnected  = false;
String savedSSID      = "";
String savedPassword  = "";
String wifiFailReason = "";

static const char* AP_SSID = "Despertador-Config";
static const char* AP_PASS = "12345678";   // minimo 8 caracteres para WPA2

// Levanta el portal de configuracion (modo dual AP + STA)
static void levantarAP() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("[WiFi] Portal AP en: http://%s\n", WiFi.softAPIP().toString().c_str());
}

// Carga las credenciales guardadas en flash
void wifiInit() {
  Preferences prefs;
  prefs.begin("wificfg", true);
  savedSSID     = prefs.getString("ssid",     "");
  savedPassword = prefs.getString("password", "");
  prefs.end();

  levantarAP();

  // El AP y el BT clasico comparten la misma antena de 2.4 GHz. Bajar un poco
  // la potencia de TX del WiFi reduce la interferencia durante el inquiry.
  WiFi.setTxPower(WIFI_POWER_11dBm);

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

    // Ya no hace falta el portal: apagarlo le devuelve tiempo de radio al
    // Bluetooth y acelera bastante el emparejamiento con el receptor.
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    Serial.println("[WiFi] AP de configuracion apagado (radio libre para BT).");

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

    // Si fallo, volvemos a dejar el portal disponible para reconfigurar
    levantarAP();
  }
}

bool wifiIsConnected() {
  // Re-verificar en tiempo real por si se cayó la conexión
  bool ahora = (WiFi.status() == WL_CONNECTED);

  // Si la conexion se cayo, volver a ofrecer el portal de configuracion
  if (wifiConnected && !ahora) {
    Serial.println("[WiFi] Conexion perdida: reactivando portal AP.");
    levantarAP();
  }

  wifiConnected = ahora;
  return wifiConnected;
}