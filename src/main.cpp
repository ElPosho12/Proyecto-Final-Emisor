#include <Arduino.h>
#include <BluetoothSerial.h>
#include "time.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "alarm_manager.h"
#include "display_manager.h"
#include <nvs_flash.h>

BluetoothSerial SerialBT;
const String SLAVE_NAME = "ESP32_Receptor";
bool conectadoBT = false;
bool btIniciado  = false;

// ─── Asignación de Pines del LED RGB ──────────────────────────────────────────
#define LED_ROJO   32
#define LED_VERDE  26
#define LED_AZUL   33

// ─── Temporización No Bloqueante ──────────────────────────────────────────────
unsigned long lastBTRetry    = 0;
unsigned long alarmStartTime = 0;
bool alarmActive = false;

// Variables para el control de los destellos momentáneos del LED
unsigned long tApagadoLedMomentaneo = 0; // 🚀 FIX: Unificada con la asignación interna
bool ledMomentaneoActivo = false;

// Estado local para saber si el usuario apagó el oxímetro remoto
bool oximetroHabilitadoEmisor = true;

// Variable de control para el redibujado del reloj
extern int lastMinute; // 🚀 FIX: Declarada la variable que faltaba en el combo

// Estados anteriores para detectar el momento exacto de la conexión
bool lastWifiState = false;
bool lastBTState   = false;

#define BT_RETRY_INTERVAL 10000   // Reintentar cada 10s si se cae

// Función auxiliar para encender un color y programar su apagado
void encenderLedMomentaneo(uint8_t pin, unsigned long duracionMs) {
  // Apagamos los tres por seguridad antes de cambiar de color
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AZUL, LOW);
  
  digitalWrite(pin, HIGH);
  tApagadoLedMomentaneo = millis() + duracionMs; // 🚀 Usamos la variable unificada
  ledMomentaneoActivo = true;
}

void btConnect() {
  Serial.println("[BT] Iniciando escaneo en el aire buscando por nombre...");
  BTScanResults* pResults = SerialBT.discover(5000);
  
  if (pResults) { 
    int count = pResults->getCount();
    for (int i = 0; i < count; i++) {
      BTAdvertisedDevice* device = pResults->getDevice(i);
      String devName = device->getName().c_str();
      if (devName == SLAVE_NAME) {
        conectadoBT = SerialBT.connect(SLAVE_NAME);
        if (conectadoBT) return;
      }
    }
  }
  conectadoBT = false;
} 

void setup() {
  Serial.begin(115200);
  
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      // Si la memoria está corrupta o no existe, la forzamos a formatearse
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  
  Serial.println("Memoria NVS Inicializada con éxito!");

  // Inicialización de Pines del LED RGB
  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AZUL, LOW);

  displayInit();
  wifiInit();
  webServerInit();
  
  // 🚀 FIX: ¡Descomentado! Ahora el administrador de alarmas arranca correctamente.
  alarmManagerInit(); 
}

void loop() {
  webServerLoop();
  unsigned long now = millis();

  // 2. Gestión del Bluetooth
  if (wifiIsConnected()) {
    if (!btIniciado) {
      SerialBT.begin("ESP32_Emisor", true);
      btIniciado  = true;
      lastBTRetry = now + 5000; 
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
    }
  }

  // Prender led azul momentáneamente al conectar Receptor y Wifi
  bool currentWifi = wifiIsConnected();
  bool currentBT   = conectadoBT;
  
  if ((currentWifi && currentBT) && (!lastWifiState || !lastBTState)) {
    encenderLedMomentaneo(LED_AZUL, 1500); // Se enciende 1,5 segundos en Azul
    Serial.println("[LED] Ambos conectados → Destello Azul.");
  }
  lastWifiState = currentWifi;
  lastBTState   = currentBT;

  // Control de apagado para los destellos momentáneos (Azul o Rojo de combo)
  if (ledMomentaneoActivo && now >= tApagadoLedMomentaneo && !alarmActive) {
    digitalWrite(LED_ROJO, LOW);
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_AZUL, LOW);
    ledMomentaneoActivo = false;
  }

  // 3. Obtener hora actual sin bloquear
  struct tm timeinfo;
  bool timeOk = getLocalTime(&timeinfo, 0);
  alarmManagerLoop(timeinfo); 
  
  if (timeOk && alarmState == STATE_ACTIVE && !alarmActive && !ledMomentaneoActivo) {
    displayClock(timeinfo, alarmHour, alarmMinute, alarmEnabled, oximetroHabilitadoEmisor, currentWifi, currentBT);
  }

  // 4. Combo +/− detectado → apagar oxímetro y prender rojo momentáneamente
  if (comboPlusMinusPressed) {
    if (conectadoBT) {
      SerialBT.write('2');
      oximetroHabilitadoEmisor = false; 
      
      // Destello Rojo momentáneo al desactivar MAX30102
      encenderLedMomentaneo(LED_ROJO, 1500);
      
      if (timeOk) {
        lastMinute = -1; 
        displayClock(timeinfo, alarmHour, alarmMinute, alarmEnabled, oximetroHabilitadoEmisor, currentWifi, currentBT);
      }
    }
    comboPlusMinusPressed = false;   
  }

  // 5. Disparar alarma y parpadeo de LED 
  if (timeOk && alarmEnabled && !alarmFired) {
    if (timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute) {
      alarmFired      = true;
      alarmActive     = true;
      alarmStartTime  = now;
      if (conectadoBT) {
        SerialBT.write('1');
        displayAlarmFired();
      }
    }
  }

  // Cuando suene la alarma, parpadea en rojo sin trabar el loop
  if (alarmActive) {
    // Alterna el LED Rojo cada 250 milisegundos
    if ((now / 250) % 2 == 0) {
      digitalWrite(LED_ROJO, HIGH);
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_AZUL, LOW);
    } else {
      digitalWrite(LED_ROJO, LOW);
    }
  }

  // 6. Apagar alarma automáticamente tras 1 minuto y regresar al menú inicial
  if (alarmActive && (now - alarmStartTime >= 60000)) {
    alarmActive = false;
    digitalWrite(LED_ROJO, LOW);
    
    if (conectadoBT) {
      SerialBT.write('0');
    }

    alarmState = STATE_CONFIRM_PREVIOUS;
    displayResetMenuState();
    displayConfirmPrevious(alarmHour, alarmMinute);
  }

  delay(1);
}