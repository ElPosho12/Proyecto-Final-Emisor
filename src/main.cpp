#include <Arduino.h>
#include <BluetoothSerial.h>
#include "time.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "alarm_manager.h"
#include "display_manager.h"

BluetoothSerial SerialBT;
const String SLAVE_NAME = "ESP32_Receptor";
bool conectadoBT = false;
bool btIniciado  = false;

// ─── Temporización No Bloqueante ──────────────────────────────────────────────
unsigned long lastBTRetry   = 0;
unsigned long alarmStartTime = 0;
bool alarmActive            = false;

#define BT_RETRY_INTERVAL 15000   // Reintentar cada 15s si se cae

void btConnect() {
  Serial.println("[BT] Iniciando escaneo en el aire buscando por nombre...");
  
  // .discover(tiempo_en_ms) realiza el escaneo. 5000ms = 5 segundos.
  // Devuelve un puntero a la lista de dispositivos encontrados.
  BTScanResults* pResults = SerialBT.discover(5000);
  
  if (pResults) { 
    int count = pResults->getCount();
    Serial.printf("[BT] Escaneo terminado. Dispositivos encontrados: %d\n", count);
    
    for (int i = 0; i < count; i++) {
      BTAdvertisedDevice* device = pResults->getDevice(i);
      String devName = device->getName().c_str();
      
      Serial.printf(" -> Dispositivo encontrado: %s\n", devName.c_str());
      
      if (devName == SLAVE_NAME) {
        Serial.println("[BT] ¡Receptor encontrado en el escaneo! Conectando...");
        conectadoBT = SerialBT.connect(SLAVE_NAME);
        displaySetBTStatus(conectadoBT);
        if (conectadoBT) {
          Serial.println("[BT] ¡Conectado con éxito por nombre!");
          return;
        }
      }
    }
  } else {
    Serial.println("[BT] El escaneo no arrojó resultados o falló el controlador.");
  }
  
  Serial.println("[BT] No se pudo conectar con el receptor en este intento.");
  conectadoBT = false;
  displaySetBTStatus(false);
} 

void setup() {
  Serial.begin(115200);
  Serial.println("\n[Main] Iniciando Despertador Sordo...");

  displayInit();
  
  // 🧪 PRUEBA DE FUEGO: Iniciamos y conectamos BT antes del WiFi
  SerialBT.begin("ESP32_Emisor", true);
  Serial.println("[TEST] Buscando receptor con WiFi totalmente apagado...");
  
  // Intentamos conectar por nombre directo (esperamos hasta 10 segundos)
  int intentos = 0;
  while (!conectadoBT && intentos < 5) {
    conectadoBT = SerialBT.connect(SLAVE_NAME);
    if (!conectadoBT) {
      Serial.println("[TEST] No lo vio, reintentando...");
      delay(2000);
      intentos++;
    }
  }
  
  if (conectadoBT) {
    Serial.println("[TEST] ¡Conectado con éxito en frío!");
  } else {
    Serial.println("[TEST] Ni siquiera en frío lo encuentra.");
  }

  // Ahora recién iniciamos el resto
  wifiInit();
  webServerInit();
  alarmManagerInit();
}

void loop() {
  // 1. Atender portal web (máxima prioridad siempre)
  webServerLoop();

  unsigned long now = millis();

  // 2. Gestión inteligente del Bluetooth ─────────────────────────────────────
// 2. Gestión inteligente del Bluetooth ─────────────────────────────────────
  if (wifiIsConnected()) {
    if (!btIniciado) {
      SerialBT.begin("ESP32_Emisor", true);
      btIniciado  = true;
      // CRÍTICO: Le damos 5 segundos de ventaja a la radio antes del primer intento
      lastBTRetry = now + 5000; 
      Serial.println("[BT] Radio encendida. Esperando estabilización para buscar por nombre...");
    }

    // Corregimos la condición para que no intente conectar en el mismo milisegundo que encendió
    if (!conectadoBT && (long)(now - lastBTRetry) >= 0) {
      lastBTRetry = now + BT_RETRY_INTERVAL; // Próximo reintento en 15s
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

  // ¡ESTO VA AFUERA! Los botones se tienen que leer SIEMPRE, haya hora o no.
  alarmManagerLoop(timeinfo); 
  
  if (timeOk) {
    // SÓLO dibujamos el reloj si la alarma ya está activa
    if (alarmState == STATE_ACTIVE) {
      displayClock(timeinfo, alarmHour, alarmMinute, alarmEnabled, !comboPlusMinusPressed);
    }
  }

  // 4. Combo +/− detectado por alarm_manager → apagar oxímetro del receptor ──
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