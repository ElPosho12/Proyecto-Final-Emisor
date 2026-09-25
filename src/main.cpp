#include <Arduino.h>
#include <BluetoothSerial.h>
#include <Preferences.h>
#include "time.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "alarm_manager.h"
#include "display_manager.h"
#include <nvs_flash.h>

// ─── Bluetooth ────────────────────────────────────────────────────────────────
BluetoothSerial SerialBT;
const char* SLAVE_NAME = "ESP32_Receptor";

// conectadoBT ahora la actualiza el callback del stack BT (otra tarea),
// por eso es volatile. alarm_manager.cpp la declara igual.
volatile bool conectadoBT = false;

// Cache de la MAC del receptor: evita el inquiry en los arranques siguientes.
static uint8_t       peerMac[6]      = {0};
static volatile bool hayMac          = false;
static volatile bool debeGuardarMac  = false;
static uint8_t       fallosSeguidos  = 0;
static TaskHandle_t  tareaBTHandle   = NULL;

#define DISCOVER_MS      4000   // ventana de inquiry (solo si no hay MAC guardada)
#define RETRY_MAC_MS     1200   // reintento cuando conectamos por MAC (barato)
#define RETRY_SCAN_MS    3000   // reintento despues de un inquiry fallido
#define MAX_FALLOS_MAC   3      // tras N fallos por MAC, volver a escanear

// ─── Asignación de Pines del LED RGB ──────────────────────────────────────────
#define LED_ROJO   26
#define LED_VERDE  32
#define LED_AZUL   33

// ─── Temporización No Bloqueante ──────────────────────────────────────────────
unsigned long alarmStartTime = 0;
bool alarmActive = false;

// Variables para el control de los destellos momentáneos del LED
unsigned long tApagadoLedMomentaneo = 0;
bool ledMomentaneoActivo = false;

// Variables para el parpadeo VERDE al confirmar la alarma
unsigned long tFinParpadeoVerde  = 0;
bool ledParpadeoVerdeActivo = false;

// Estado local para saber si el usuario apagó el oxímetro remoto
bool oximetroHabilitadoEmisor = true;

// Variable de control para el redibujado del reloj
extern int lastMinute;

// Estados anteriores para detectar el momento exacto de la conexión
bool lastWifiState = false;
bool lastBTState   = false;

// Avisos temporales de pantallas completas
#define MENSAJE_DURACION_MS  2500

bool          mostrandoMensajeWifi = false;
unsigned long tFinMensajeWifi      = 0;

bool          mostrandoMensajeBT   = false;
unsigned long tFinMensajeBT       = 0;

// Instrucciones iniciales de configuración de WiFi: solo aparecen si todavía
// no hay SSID/clave guardados, y solo una vez al arrancar, durante 10 s.
#define INSTRUCCIONES_WIFI_DURACION_MS  10000
bool          mostrandoInstruccionesWifi = false;
unsigned long tFinInstruccionesWifi      = 0;

// Franja inferior "Buscando WiFi.../Bluetooth...": se redibuja cada 200 ms
#define ESTADO_BUSQUEDA_INTERVALO_MS 200
unsigned long tUltimoEstadoBusqueda = 0;

// ─── Funciones auxiliares del LED ─────────────────────────────────────────────
void encenderLedMomentaneo(uint8_t pin, unsigned long duracionMs) {
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AZUL, LOW);

  digitalWrite(pin, HIGH);
  tApagadoLedMomentaneo = millis() + duracionMs;
  ledMomentaneoActivo = true;
}

void iniciarParpadeoVerde(unsigned long duracionMs) {
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_AZUL, LOW);
  tFinParpadeoVerde = millis() + duracionMs;
  ledParpadeoVerdeActivo = true;
}

// ─── Persistencia de la MAC del receptor ──────────────────────────────────────
static void cargarMacDesdeNVS() {
  Preferences p;
  p.begin("btlink", true);
  size_t n = p.getBytes("peer", peerMac, 6);
  p.end();
  hayMac = (n == 6);

  if (hayMac) {
    Serial.printf("[BT] MAC del receptor en cache: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  peerMac[0], peerMac[1], peerMac[2],
                  peerMac[3], peerMac[4], peerMac[5]);
  } else {
    Serial.println("[BT] Sin MAC cacheada: el primer arranque hara un inquiry.");
  }
}

static void guardarMacEnNVS() {
  Preferences p;
  p.begin("btlink", false);
  p.putBytes("peer", peerMac, 6);
  p.end();
  Serial.println("[BT] MAC del receptor guardada en NVS.");
}

// ─── Callback del stack SPP: estado de conexión sin polling ───────────────────
static void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
  if (event == ESP_SPP_OPEN_EVT) {                  // conexion saliente abierta
    if (param->open.status != ESP_SPP_SUCCESS) return;
    conectadoBT    = true;
    fallosSeguidos = 0;
    memcpy(peerMac, param->open.rem_bda, 6);
    hayMac         = true;
    debeGuardarMac = true;                          // la escritura ocurre en loop()
    Serial.println("[BT] Conectado al receptor.");

  } else if (event == ESP_SPP_SRV_OPEN_EVT) {       // conexion entrante
    if (param->srv_open.status != ESP_SPP_SUCCESS) return;
    conectadoBT    = true;
    fallosSeguidos = 0;
    memcpy(peerMac, param->srv_open.rem_bda, 6);
    hayMac         = true;
    debeGuardarMac = true;
    Serial.println("[BT] Receptor conectado (entrante).");

  } else if (event == ESP_SPP_CLOSE_EVT) {
    conectadoBT = false;
    Serial.println("[BT] Conexion cerrada.");
  }
}

// ─── Inquiry: solo cuando no tenemos una MAC util ─────────────────────────────
static bool buscarPorNombreYConectar() {
  Serial.println("[BT] Inquiry buscando el receptor por nombre...");
  BTScanResults* res = SerialBT.discover(DISCOVER_MS);
  if (!res) return false;

  int count = res->getCount();
  for (int i = 0; i < count; i++) {
    BTAdvertisedDevice* dev = res->getDevice(i);
    if (String(dev->getName().c_str()) != String(SLAVE_NAME)) continue;

    BTAddress addr = dev->getAddress();
    Serial.printf("[BT] Encontrado en %s, conectando...\n", addr.toString().c_str());
    return SerialBT.connect(*addr.getNative());
  }

  Serial.println("[BT] El receptor no aparecio en este inquiry.");
  return false;
}

// ─── Tarea de conexión (core 0): no bloquea el loop de Arduino ────────────────
static void tareaConexionBT(void* /*arg*/) {
  for (;;) {
    if (conectadoBT) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    bool ok = false;

    if (hayMac && fallosSeguidos < MAX_FALLOS_MAC) {
      ok = SerialBT.connect(peerMac);     // page directo, sin inquiry
      if (!ok) {
        fallosSeguidos++;
        Serial.printf("[BT] Fallo la conexion por MAC (%u/%u).\n",
                      fallosSeguidos, (unsigned)MAX_FALLOS_MAC);
      }
    } else {
      ok = buscarPorNombreYConectar();
      if (ok) fallosSeguidos = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(ok ? 300 : (hayMac ? RETRY_MAC_MS : RETRY_SCAN_MS)));
  }
}

void setup() {
  Serial.begin(115200);

  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  Serial.println("Memoria NVS Inicializada con exito!");

  // Inicialización de Pines del LED RGB
  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AZUL, LOW);

  displayInit();

  cargarMacDesdeNVS();
  SerialBT.register_callback(btCallback);
  SerialBT.begin("ESP32_Emisor", true);   // true = master
  xTaskCreatePinnedToCore(tareaConexionBT, "BTLink", 4096, NULL, 1, &tareaBTHandle, 0);

  wifiInit();
  webServerInit();

  alarmManagerInit();

  // Si todavia no hay credenciales de WiFi guardadas, tapamos la pantalla
  // inicial con las instrucciones de configuracion durante 10 segundos.
  if (savedSSID == "") {
    displayInstruccionesWifi();
    mostrandoInstruccionesWifi = true;
    tFinInstruccionesWifi = millis() + INSTRUCCIONES_WIFI_DURACION_MS;
    Serial.println("[WiFi] Sin credenciales guardadas: mostrando instrucciones de configuracion.");
  }
}

void loop() {
  webServerLoop();
  unsigned long now = millis();

  // Escritura diferida de la MAC
  if (debeGuardarMac) {
    debeGuardarMac = false;
    guardarMacEnNVS();
  }

  bool currentWifi = wifiIsConnected();
  bool currentBT   = conectadoBT;

  // Flanco de conexión exitosa del WiFi
  if (currentWifi && !lastWifiState) {
    displayWifiConectado();
    mostrandoMensajeWifi = true;
    tFinMensajeWifi = now + MENSAJE_DURACION_MS;
    encenderLedMomentaneo(LED_VERDE, MENSAJE_DURACION_MS);
    Serial.println("[WiFi] Conectado: mostrando aviso en pantalla.");
  }

  // Flanco de conexión exitosa del Bluetooth
  if (currentBT && !lastBTState) {
    if (!mostrandoMensajeWifi && !mostrandoInstruccionesWifi) {
      displayBtConectado();
      mostrandoMensajeBT = true;
      tFinMensajeBT = now + MENSAJE_DURACION_MS;
    }

    if (alarmState == STATE_ACTIVE) {
      SerialBT.write('3');
      Serial.println("[BT] Reconexion con alarma ya confirmada: reenviando '3'.");
    }
  }

  if ((currentWifi && currentBT) && (!lastWifiState || !lastBTState)) {
    encenderLedMomentaneo(LED_AZUL, 1500);
    Serial.println("[LED] Ambos conectados -> Destello Azul.");
  }

  lastWifiState = currentWifi;
  lastBTState   = currentBT;

  // Franja "Buscando WiFi.../Bluetooth..." abajo de la pantalla
  if (!mostrandoInstruccionesWifi && !mostrandoMensajeWifi && !mostrandoMensajeBT && !alarmActive &&
      (now - tUltimoEstadoBusqueda >= ESTADO_BUSQUEDA_INTERVALO_MS)) {
    tUltimoEstadoBusqueda = now;
    displayEstadoBusqueda(currentWifi, currentBT);
  }

  // Control de apagado para los destellos momentáneos
  if (ledMomentaneoActivo && now >= tApagadoLedMomentaneo && !alarmActive) {
    digitalWrite(LED_ROJO, LOW);
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_AZUL, LOW);
    ledMomentaneoActivo = false;
  }

  struct tm timeinfo;
  bool timeOk = getLocalTime(&timeinfo, 0);

  // Control de temporización de avisos temporales en pantalla
  if (mostrandoInstruccionesWifi) {
    if (now >= tFinInstruccionesWifi) {
      mostrandoInstruccionesWifi = false;
      // Si mientras tanto ya se disparo algun otro aviso, lo dejamos seguir
      // su propio timer; si no, restauramos la pantalla normal del estado
      // actual de la alarma.
      if (!mostrandoMensajeWifi && !mostrandoMensajeBT) {
        alarmManagerRedraw(timeinfo, oximetroHabilitadoEmisor, currentWifi, currentBT);
      }
      Serial.println("[WiFi] Instrucciones terminadas: pantalla restaurada.");
    }
  }
  else if (mostrandoMensajeWifi) {
    if (now >= tFinMensajeWifi) {
      mostrandoMensajeWifi = false;
      if (mostrandoMensajeBT) {
        displayBtConectado();
        tFinMensajeBT = now + MENSAJE_DURACION_MS;
      } else {
        alarmManagerRedraw(timeinfo, oximetroHabilitadoEmisor, currentWifi, currentBT);
        Serial.println("[WiFi] Aviso terminado: pantalla restaurada.");
      }
    }
  } 
  else if (mostrandoMensajeBT) {
    if (now >= tFinMensajeBT) {
      mostrandoMensajeBT = false;
      alarmManagerRedraw(timeinfo, oximetroHabilitadoEmisor, currentWifi, currentBT);
      Serial.println("[BT] Aviso terminado: pantalla restaurada.");
    }
  } 
  else {
    alarmManagerLoop(timeinfo);

    if (timeOk && alarmState == STATE_ACTIVE && !alarmActive && !ledMomentaneoActivo) {
      displayClock(timeinfo, alarmHour, alarmMinute, alarmEnabled, oximetroHabilitadoEmisor, currentWifi, currentBT);
    }
  }

  // Alarma recién confirmada → habilitar mediciones en el receptor
  if (alarmaRecienConfirmada) {
    if (currentBT) {
      SerialBT.write('3');
      Serial.println("[BT] Alarma confirmada: enviado '3' (habilitar mediciones).");
    }
    alarmaRecienConfirmada = false;
  }

  // Combo +/− detectado → apagar oxímetro y prender rojo momentáneamente
  if (comboPlusMinusPressed) {
    if (currentBT) {
      SerialBT.write('2');
      oximetroHabilitadoEmisor = false;

      encenderLedMomentaneo(LED_ROJO, 1500);

      if (timeOk) {
        lastMinute = -1;
        displayClock(timeinfo, alarmHour, alarmMinute, alarmEnabled, oximetroHabilitadoEmisor, currentWifi, currentBT);
      }
    }
    comboPlusMinusPressed = false;
  }

  // Disparar alarma y parpadeo de LED
  if (timeOk && alarmEnabled && !alarmFired) {
    if (timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute) {
      alarmFired      = true;
      alarmActive     = true;
      alarmStartTime  = now;
      if (currentBT) {
        SerialBT.write('1');
      }
      displayAlarmFired();
    }
  }

  // Parpadeo VERDE al confirmar la alarma
  if (ledParpadeoVerdeActivo) {
    if ((now / 250) % 2 == 0) {
      digitalWrite(LED_VERDE, HIGH);
    } else {
      digitalWrite(LED_VERDE, LOW);
    }
    if (now >= tFinParpadeoVerde) {
      ledParpadeoVerdeActivo = false;
      digitalWrite(LED_VERDE, LOW);
    }
  }

  // Cuando suene la alarma, parpadea en rojo sin trabar el loop
  if (alarmActive) {
    if ((now / 250) % 2 == 0) {
      digitalWrite(LED_ROJO, HIGH);
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_AZUL, LOW);
    } else {
      digitalWrite(LED_ROJO, LOW);
    }
  }

  // Apagar alarma automáticamente tras 1 minuto y volver al menú inicial
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