#include <Arduino.h>
#include <BluetoothSerial.h>
#include <Preferences.h>
#include "time.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "alarm_manager.h"
#include "display_manager.h"
#include <nvs_flash.h>

// Datos del receptor que buscara por bluetooth
BluetoothSerial SerialBT;
const char* Receptor = "ESP32_Receptor";

// Se asegura de inicializar en '0' la variable "conectadoBT"
volatile bool conectadoBT = false;

// Cache de la MAC del receptor
static uint8_t       peerMac[6]      = {0};
static volatile bool hayMac          = false;
static volatile bool debeGuardarMac  = false;
static uint8_t       fallosSeguidos  = 0;
static TaskHandle_t  tareaBTHandle   = NULL;

#define DISCOVER_MS      4000   // ventana de inquiry
#define RETRY_MAC_MS     1200   // reintento por MAC
#define RETRY_SCAN_MS    3000   // reintento tras inquiry fallido
#define MAX_FALLOS_MAC   3      // fallos antes de re-escanear

// LEDs
#define LED_ROJO   26
#define LED_VERDE  32
#define LED_AZUL   33

// Temporizacion no bloqueante
unsigned long TiempoAlarma = 0;
bool InicioAlarma = false;

// Destellos momentaneos del LED
unsigned long tApagadoLedMomentaneo = 0;
bool ledMomentaneoActivo = false;

// Parpadeo verde al confirmar alarma
unsigned long tFinParpadeoVerde = 0;
bool ledParpadeoVerdeActivo = false;

// Estado local del oximetro remoto
bool oximetroHabilitadoEmisor = true;

extern int UltimoMinuto;

// Estados anteriores para detectar flancos de conexion
bool lastWifiState = false;
bool lastBTState   = false;

// Avisos temporales de pantalla completa
#define MENSAJE_DURACION_MS  2500

bool mostrandoMensajeWifi = false;
unsigned long tFinMensajeWifi      = 0;

bool mostrandoMensajeBT = false;
unsigned long tFinMensajeBT      = 0;

// Instrucciones iniciales de WiFi (solo si no hay SSID guardado)
#define INSTRUCCIONES_WIFI_DURACION_MS  10000
bool mostrandoInstruccionesWifi = false;
unsigned long tFinInstruccionesWifi      = 0;

// Franja "Buscando WiFi/Bluetooth"
#define ESTADO_BUSQUEDA_INTERVALO_MS 200
unsigned long tUltimoEstadoBusqueda = 0;

// Prende un LED por un tiempo determinado
void encenderLedMomentaneo(uint8_t pin, unsigned long duracionMs) {
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AZUL, LOW);

  digitalWrite(pin, HIGH);
  tApagadoLedMomentaneo = millis() + duracionMs;
  ledMomentaneoActivo = true;
}

// Arranca el parpadeo verde
void iniciarParpadeoVerde(unsigned long duracionMs) {
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_AZUL, LOW);
  tFinParpadeoVerde = millis() + duracionMs;
  ledParpadeoVerdeActivo = true;
}

// Carga la MAC del receptor guardada en NVS
static void cargarMacDesdeNVS() {
  Preferences p;
  p.begin("btlink", true);
  size_t n = p.getBytes("peer", peerMac, 6);
  p.end();
  hayMac = (n == 6);

  if (hayMac) {
    Serial.printf("[BT] MAC en cache: %02X:%02X:%02X:%02X:%02X:%02X\n",
    peerMac[0], peerMac[1], peerMac[2],
    peerMac[3], peerMac[4], peerMac[5]);
  } else {
    Serial.println("[BT] Sin MAC cacheada.");
  }
}

// Guarda la MAC del receptor en NVS
static void guardarMacEnNVS() {
  Preferences p;
  p.begin("btlink", false);
  p.putBytes("peer", peerMac, 6);
  p.end();
  Serial.println("[BT] MAC guardada.");
}

// Callback de eventos del stack Bluetooth
static void manejarEventoBT(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
  if (event == ESP_SPP_OPEN_EVT) {
    if (param->open.status != ESP_SPP_SUCCESS) return;
    conectadoBT    = true;
    fallosSeguidos = 0;
    memcpy(peerMac, param->open.rem_bda, 6);
    hayMac         = true;
    debeGuardarMac = true;
    Serial.println("[BT] Conectado.");

  } else if (event == ESP_SPP_SRV_OPEN_EVT) {
    if (param->srv_open.status != ESP_SPP_SUCCESS) return;
    conectadoBT    = true;
    fallosSeguidos = 0;
    memcpy(peerMac, param->srv_open.rem_bda, 6);
    hayMac         = true;
    debeGuardarMac = true;
    Serial.println("[BT] Conectado (entrante).");

  } else if (event == ESP_SPP_CLOSE_EVT) {
    conectadoBT = false;
    Serial.println("[BT] Desconectado.");
  }
}

// Busca al receptor por nombre y conecta
static bool buscarPorNombreYConectar() {
  Serial.println("[BT] Buscando receptor...");
  BTScanResults* res = SerialBT.discover(DISCOVER_MS);
  if (!res) return false;

  int count = res->getCount();
  for (int i = 0; i < count; i++) {
    BTAdvertisedDevice* dev = res->getDevice(i);
    if (String(dev->getName().c_str()) != String(Receptor)) continue;

    BTAddress addr = dev->getAddress();
    Serial.printf("[BT] Encontrado en %s.\n", addr.toString().c_str());
    return SerialBT.connect(*addr.getNative());
  }

  Serial.println("[BT] No se encontro el receptor.");
  return false;
}

// Tarea de conexion BT en el core 0
static void tareaConexionBT(void* /*arg*/) {
  for (;;) {
    if (conectadoBT) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    bool ok = false;

    if (hayMac && fallosSeguidos < MAX_FALLOS_MAC) {
      ok = SerialBT.connect(peerMac);
      if (!ok) {
        fallosSeguidos++;
        Serial.printf("[BT] Fallo conexion por MAC (%u/%u).\n",
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
  pinMode(BTN_ENTER, INPUT_PULLUP);
  delay(50);
  Serial.printf("[DIAG] GPIO25 (ENTER) al arrancar: %s\n", 
                digitalRead(BTN_ENTER) == LOW ? "LOW (presionado)" : "HIGH (suelto)");
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  Serial.println("NVS inicializada.");

  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AZUL, LOW);

  iniciarPantalla();

  cargarMacDesdeNVS();
  SerialBT.register_callback(manejarEventoBT);
  SerialBT.begin("ESP32_Emisor", true);
  xTaskCreatePinnedToCore(tareaConexionBT, "BTLink", 4096, NULL, 1, &tareaBTHandle, 0);

  iniciarWifi();
  iniciarServidorWeb();

  iniciarAdministradorAlarma();

  // Si no hay WiFi guardado, mostrar instrucciones por 10s
  if (savedSSID == "") {
    mostrarInstruccionesWifi();
    mostrandoInstruccionesWifi = true;
    tFinInstruccionesWifi = millis() + INSTRUCCIONES_WIFI_DURACION_MS;
    Serial.println("[WiFi] Sin credenciales: mostrando instrucciones.");
  }
}

void loop() {
  actualizarServidorWeb();
  unsigned long now = millis();

  if (debeGuardarMac) {
    debeGuardarMac = false;
    guardarMacEnNVS();
  }

  bool currentWifi = wifiEstaConectado();
  bool currentBT   = conectadoBT;

  // Flanco de conexion WiFi
  if (currentWifi && !lastWifiState) {
    mostrarWifiConectado();
    mostrandoMensajeWifi = true;
    tFinMensajeWifi = now + MENSAJE_DURACION_MS;
    encenderLedMomentaneo(LED_VERDE, MENSAJE_DURACION_MS);
    Serial.println("[WiFi] Conectado.");
  }

  // Flanco de conexion Bluetooth
  if (currentBT && !lastBTState) {
    if (!mostrandoMensajeWifi && !mostrandoInstruccionesWifi) {
      mostrarBtConectado();
      mostrandoMensajeBT = true;
      tFinMensajeBT = now + MENSAJE_DURACION_MS;
    }

    if (alarmState == STATE_ACTIVE) {
      SerialBT.write('3');
      Serial.println("[BT] Reenviando confirmacion de alarma.");
    }
  }

  if ((currentWifi && currentBT) && (!lastWifiState || !lastBTState)) {
    encenderLedMomentaneo(LED_AZUL, 1500);
    Serial.println("[LED] Ambos conectados.");
  }

  lastWifiState = currentWifi;
  lastBTState   = currentBT;

  // Franja "Buscando WiFi/Bluetooth"
  if (!mostrandoInstruccionesWifi && !mostrandoMensajeWifi && !mostrandoMensajeBT && !InicioAlarma &&
      (now - tUltimoEstadoBusqueda >= ESTADO_BUSQUEDA_INTERVALO_MS)) {
    tUltimoEstadoBusqueda = now;
    mostrarEstadoBusqueda(currentWifi, currentBT);
  }

  // Apagado de destellos momentaneos
  if (ledMomentaneoActivo && now >= tApagadoLedMomentaneo && !InicioAlarma) {
    digitalWrite(LED_ROJO, LOW);
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_AZUL, LOW);
    ledMomentaneoActivo = false;
  }

  struct tm timeinfo;
  bool timeOk = getLocalTime(&timeinfo, 0);

  // Prioridad de avisos temporales en pantalla
  if (mostrandoInstruccionesWifi) {
    if (now >= tFinInstruccionesWifi) {
      mostrandoInstruccionesWifi = false;
      if (!mostrandoMensajeWifi && !mostrandoMensajeBT) {
        redibujarAdministradorAlarma(timeinfo, oximetroHabilitadoEmisor, currentWifi, currentBT);
      }
      Serial.println("[WiFi] Instrucciones terminadas.");
    }
  }
  else if (mostrandoMensajeWifi) {
    if (now >= tFinMensajeWifi) {
      mostrandoMensajeWifi = false;
      if (mostrandoMensajeBT) {
        mostrarBtConectado();
        tFinMensajeBT = now + MENSAJE_DURACION_MS;
      } else {
        redibujarAdministradorAlarma(timeinfo, oximetroHabilitadoEmisor, currentWifi, currentBT);
        Serial.println("[WiFi] Aviso terminado.");
      }
    }
  }
  else if (mostrandoMensajeBT) {
    if (now >= tFinMensajeBT) {
      mostrandoMensajeBT = false;
      redibujarAdministradorAlarma(timeinfo, oximetroHabilitadoEmisor, currentWifi, currentBT);
      Serial.println("[BT] Aviso terminado.");
    }
  }
  else {
    actualizarAdministradorAlarma(timeinfo);

    if (timeOk && alarmState == STATE_ACTIVE && !InicioAlarma && !ledMomentaneoActivo) {
      mostrarReloj(timeinfo, alarmHour, alarmMinute, alarmEnabled, oximetroHabilitadoEmisor, currentWifi, currentBT);
    }
  }

  // Alarma confirmada: habilitar mediciones en el receptor
  if (alarmaRecienConfirmada) {
    if (currentBT) {
      SerialBT.write('3');
      Serial.println("[BT] Mediciones habilitadas.");
    }
    alarmaRecienConfirmada = false;
  }

  // Combo +/-: apagar oximetro
  if (comboPlusMinusPressed) {
    if (currentBT) {
      SerialBT.write('2');
      oximetroHabilitadoEmisor = false;

      encenderLedMomentaneo(LED_ROJO, 1500);

      if (timeOk) {
        UltimoMinuto = -1;
        mostrarReloj(timeinfo, alarmHour, alarmMinute, alarmEnabled, oximetroHabilitadoEmisor, currentWifi, currentBT);
      }
    }
    comboPlusMinusPressed = false;
  }

  // Disparo de alarma
  if (timeOk && alarmEnabled && !alarmFired) {
    if (timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute) {
      alarmFired      = true;
      InicioAlarma     = true;
      TiempoAlarma  = now;
      if (currentBT) {
        SerialBT.write('1');
      }
      mostrarAlarmaSonando();
    }
  }

  // Parpadeo verde de confirmacion
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

  // Parpadeo rojo mientras suena la alarma
  if (InicioAlarma) {
    if ((now / 250) % 2 == 0) {
      digitalWrite(LED_ROJO, HIGH);
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_AZUL, LOW);
    } else {
      digitalWrite(LED_ROJO, LOW);
    }
  }

  // Apagado automatico tras 1 minuto
  if (InicioAlarma && (now - TiempoAlarma >= 60000)) {
    InicioAlarma = false;
    digitalWrite(LED_ROJO, LOW);

    if (conectadoBT) {
      SerialBT.write('0');
    }

    alarmState = STATE_CONFIRM_PREVIOUS;
    reiniciarEstadoMenu();
    mostrarConfirmarAnterior(alarmHour, alarmMinute);
  }

  delay(1);
}