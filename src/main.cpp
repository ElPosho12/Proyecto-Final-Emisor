#include <Arduino.h>
#include <BluetoothSerial.h>
#include "time.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "alarm_manager.h"
#include "display_manager.h"

BluetoothSerial SerialBT;
const String SLAVE_NAME = "ESP32_Receptor_LED";
bool conectadoBT = false;
bool btIniciado  = false;

// ─── Temporización No Bloqueante ──────────────────────────────────────────────
unsigned long lastBTRetry   = 0;
unsigned long alarmStartTime = 0;
bool alarmActive            = false;

#define BT_RETRY_INTERVAL 15000   // Reintentar cada 15s si se cae

void btConnect() {
  Serial.println("[BT] Buscando receptor en el aire...");
  conectadoBT = SerialBT.connect(SLAVE_NAME);
  displaySetBTStatus(conectadoBT);
  if (conectadoBT) {
    Serial.println("[BT] ¡Conectado con éxito a la pulsera!");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n[Main] Iniciando Despertador Sordo...");

  displayInit();
  wifiInit();
  webServerInit();
  alarmManagerInit();

  // ⚠️ NO iniciamos el Bluetooth acá. Dejamos la radio 100% libre para el WiFi.
  Serial.println("[Main] Setup completo. Portal Web listo y protegido.");
}

void loop() {
  // 1. Atender portal web (máxima prioridad siempre)
  webServerLoop();

  unsigned long now = millis();

  // 2. Gestión inteligente del Bluetooth ─────────────────────────────────────
  if (wifiIsConnected()) {
    if (!btIniciado) {
      SerialBT.begin("ESP32_Emisor", true);
      btIniciado  = true;
      lastBTRetry = now;
      Serial.println("[BT] WiFi detectado. Radio Bluetooth encendida de fondo.");
    }

    if (!conectadoBT && (now - lastBTRetry >= BT_RETRY_INTERVAL)) {
      lastBTRetry = now;
      btConnect();
    }
  } else {
    // Sin WiFi → apagar BT para liberar antena al portal web
    if (btIniciado) {
      SerialBT.end();
      btIniciado  = false;
      conectadoBT = false;
      displaySetBTStatus(false);
      Serial.println("[BT] Sin WiFi de confianza. Bluetooth apagado para priorizar el Portal Web.");
    }
  }

  // 3. Obtener hora actual sin bloquear (solo si hay WiFi/NTP)
  struct tm timeinfo;
  bool timeOk = getLocalTime(&timeinfo, 0);

  if (timeOk) {
    alarmManagerLoop(timeinfo);
  }

  // 4. Combo +/− detectado por alarm_manager → apagar oxímetro del receptor ──
  // alarm_manager.cpp pone comboPlusMinusPressed = true cuando detecta el combo
  // en STATE_ACTIVE. Acá lo leemos, enviamos '2' por BT y bajamos la bandera.
  if (comboPlusMinusPressed) {
    if (conectadoBT) {
      SerialBT.write('2');
      Serial.println("[BT] Enviado '2' → oxímetro desactivado en receptor.");
    } else {
      Serial.println("[BT] Combo +/- detectado pero receptor no conectado. '2' no enviado.");
    }
    comboPlusMinusPressed = false;   // bajar siempre, conectado o no
  }

  // 5. Disparar alarma (no bloqueante) ────────────────────────────────────────
  if (timeOk && alarmEnabled && !alarmFired) {
    if (timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute) {
      alarmFired      = true;
      alarmActive     = true;
      alarmStartTime  = now;
      Serial.println("[Alarm] ¡Disparando alarma!");

      if (conectadoBT) {
        SerialBT.write('1');
        displayAlarmFired();
      } else {
        Serial.println("[Alarm] Receptor BT no conectado.");
      }
    }
  }

  // 6. Apagar alarma automáticamente tras 1 minuto ────────────────────────────
  if (alarmActive && (now - alarmStartTime >= 60000)) {
    alarmActive = false;
    Serial.println("[Alarm] Fin del tiempo de alarma. Apagando...");
    if (conectadoBT) {
      SerialBT.write('0');
    }
  }

  delay(1);
} 