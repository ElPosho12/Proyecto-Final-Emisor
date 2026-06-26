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
unsigned long lastBTRetry    = 0;
unsigned long alarmStartTime = 0;
bool alarmActive             = false;

// Estado local para saber si el usuario apagó el oxímetro remoto
bool oximetroHabilitadoEmisor = true;

#define BT_RETRY_INTERVAL 15000   // Reintentar cada 15s si se cae

void btConnect() {
  Serial.println("[BT] Iniciando escaneo en el aire buscando por nombre...");
  
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
} 

void setup() {
  Serial.begin(115200);
  Serial.println("\n[Main] Iniciando Despertador Sordo...");

  displayInit();
  wifiInit();
  webServerInit();
  alarmManagerInit();

  Serial.println("[Main] Todo el sistema base arrancó en paralelo. Buscando BT en background...");
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
      lastBTRetry = now + 5000; 
      Serial.println("[BT] Radio encendida. Esperando estabilización para buscar por nombre...");
    }

    if (!conectadoBT && (long)(now - lastBTRetry) >= 0) {
      lastBTRetry = now + BT_RETRY_INTERVAL; 
      btConnect();
    } 
  } else {
    if (btIniciado) {
      SerialBT.end();
      btIniciado  = false;
      conectadoBT = false;
      Serial.println("[BT] Sin WiFi de confianza. Bluetooth apagado para priorizar el Portal Web.");
    }
  }

  // 3. Obtener hora actual sin bloquear (solo si hay WiFi/NTP)
// 3. Obtener hora actual sin bloquear (solo si hay WiFi/NTP)
  struct tm timeinfo;
  bool timeOk = getLocalTime(&timeinfo, 0);

  alarmManagerLoop(timeinfo); 
  
  if (timeOk) {
    if (alarmState == STATE_ACTIVE) {
      // 🚀 CORREGIDO: Reemplazamos el !comboPlusMinusPressed por nuestra variable de estado persistente
      displayClock(timeinfo, alarmHour, alarmMinute, alarmEnabled, oximetroHabilitadoEmisor, wifiIsConnected(), conectadoBT);
    }
  }

  // 4. Combo +/− detectado por alarm_manager → apagar oxímetro del receptor ──
  if (comboPlusMinusPressed) {
    if (conectadoBT) {
      SerialBT.write('2');
      oximetroHabilitadoEmisor = false; // 👈 Guarda el estado apagado de forma permanente
      Serial.println("[BT] Enviado '2' → oxímetro desactivado en receptor.");
      
      // Forzar el refresco de pantalla inmediato rompiendo el filtro de minutos
      lastMinute = -1; 
    } else {
      Serial.println("[BT] Combo +/- detectado pero receptor no conectado. '2' no enviado.");
    }
    comboPlusMinusPressed = false;   
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
  // 6. Apagar alarma automáticamente tras 1 minuto y regresar al menú inicial ───
  if (alarmActive && (now - alarmStartTime >= 60000)) {
    alarmActive = false;
    Serial.println("[Alarm] Fin del tiempo de alarma. Apagando...");
    
    if (conectadoBT) {
      SerialBT.write('0');
    }

    // 🚀 NUEVO: Forzar el regreso al estado de confirmación de alarma anterior
    alarmState = STATE_CONFIRM_PREVIOUS;
    
    // Limpiamos los estados de las vistas del menú para que permita redibujar desde cero
    displayResetMenuState();
    
    // Llamamos a la función gráfica que dibuja la pantalla de consulta de alarma vieja
    displayConfirmPrevious(alarmHour, alarmMinute);
    
    Serial.println("[Alarm] Regresando automáticamente al menú: ¿Usar alarma anterior?");
  }

  delay(1);
}