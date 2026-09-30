#include "alarm_manager.h"
#include "display_manager.h"
#include <Preferences.h>
#include "wifi_manager.h"
#include <TFT_eSPI.h>

extern TFT_eSPI tft;

#define LED_ROJO   26
#define LED_VERDE  32
#define LED_AZUL   33

// La actualiza el callback del stack BT (otra tarea)
extern volatile bool conectadoBT;
extern bool oximetroHabilitadoEmisor;

AlarmState alarmState   = STATE_CONFIRM_PREVIOUS;
int  alarmHour          = 7;
int  alarmMinute        = 0;
bool alarmFired         = false;
bool alarmEnabled       = false;

bool comboPlusMinusPressed  = false;
bool alarmaRecienConfirmada = false;

// Valores temporales mientras se configura
static int  tempHour    = 7;
static int  tempMinute  = 0;
static bool hasPrevious = false;

// Control de pulsadores
static unsigned long buttonPressedTimePlus  = 0;
static unsigned long buttonPressedTimeMinus = 0;
static unsigned long lastActionTime         = 0;
static unsigned long lastPressEnter         = 0;
static bool          enterEstabaPresionado  = false;

#define LONG_PRESS_DELAY  500   // ms para pulsacion larga
#define REPEAT_INTERVAL   120   // ms entre repeticiones

// Combo +/- simultaneo
#define COMBO_HOLD_MS  150

static unsigned long comboStartTime = 0;
static bool          comboFired     = false;

void encenderLedMomentaneo(uint8_t pin, unsigned long duracionMs);
void iniciarParpadeoVerde(unsigned long duracionMs);

// LED azul fijo mientras se configura la alarma
static void encenderLedAzulFijo() {
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AZUL, HIGH);
}

static bool detectarCombo() {
  bool plusLow  = (digitalRead(BTN_PLUS)  == LOW);
  bool minusLow = (digitalRead(BTN_MINUS) == LOW);

  if (plusLow && minusLow) {
    if (comboStartTime == 0) {
      comboStartTime = millis();
    }
    if (!comboFired && (millis() - comboStartTime >= COMBO_HOLD_MS)) {
      comboFired = true;
      return true;
    }
  } else {
    comboStartTime = 0;
    comboFired     = false;
  }
  return false;
}

// Boton "+" con pulsacion larga y repeticion
static bool manejarMasFluido() {
  if (digitalRead(BTN_MINUS) == LOW) return false;

  unsigned long now = millis();
  if (digitalRead(BTN_PLUS) == LOW) {
    if (buttonPressedTimePlus == 0) {
      buttonPressedTimePlus = now;
      lastActionTime = now;
      return true;
    } else if (now - buttonPressedTimePlus > LONG_PRESS_DELAY) {
      if (now - lastActionTime > REPEAT_INTERVAL) {
        lastActionTime = now;
        return true;
      }
    }
  } else {
    buttonPressedTimePlus = 0;
  }
  return false;
}

// Boton "-" con pulsacion larga y repeticion
static bool manejarMenosFluido() {
  if (digitalRead(BTN_PLUS) == LOW) return false;

  unsigned long now = millis();
  if (digitalRead(BTN_MINUS) == LOW) {
    if (buttonPressedTimeMinus == 0) {
      buttonPressedTimeMinus = now;
      lastActionTime = now;
      return true;
    } else if (now - buttonPressedTimeMinus > LONG_PRESS_DELAY) {
      if (now - lastActionTime > REPEAT_INTERVAL) {
        lastActionTime = now;
        return true;
      }
    }
  } else {
    buttonPressedTimeMinus = 0;
  }
  return false;
}

// Antirrebote de ENTER, no bloqueante
static bool seDetectoEnter() {
  unsigned long now = millis();
  bool presionado = (digitalRead(BTN_ENTER) == LOW);

  if (presionado && !enterEstabaPresionado && (now - lastPressEnter > 300)) {
    enterEstabaPresionado = true;
    lastPressEnter = now;
    return true;
  }
  if (!presionado) {
    enterEstabaPresionado = false;
  }
  return false;
}

// Guarda la alarma en flash
static void guardarAlarmaEnFlash() {
  Preferences prefs;
  prefs.begin("alarm", false);
  prefs.putInt("hour",    alarmHour);
  prefs.putInt("minute",  alarmMinute);
  prefs.putBool("exists", true);
  prefs.end();
}

// Carga la alarma guardada desde flash
static bool cargarAlarmaDesdeFlash() {
  Preferences prefs;
  prefs.begin("alarm", false);

  bool exists = prefs.getBool("exists", false);
  if (exists) {
    alarmHour   = prefs.getInt("hour",   7);
    alarmMinute = prefs.getInt("minute", 0);
  }
  prefs.end();
  return exists;
}

// Inicializa pulsadores y carga la alarma guardada
void iniciarAdministradorAlarma() {
  pinMode(BTN_PLUS,  INPUT_PULLUP);
  pinMode(BTN_MINUS, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);

  hasPrevious = cargarAlarmaDesdeFlash();

  if (hasPrevious) {
    alarmState = STATE_CONFIRM_PREVIOUS;
    mostrarConfirmarAnterior(alarmHour, alarmMinute);
  } else {
    tempHour   = 7;
    tempMinute = 0;
    alarmState = STATE_SET_HOUR;
    encenderLedAzulFijo();
    mostrarConfigurarHora(tempHour);
  }
}

// Bucle principal de la maquina de estados
void actualizarAdministradorAlarma(struct tm timeinfo) {
  if (alarmState == STATE_ACTIVE && detectarCombo()) {
    comboPlusMinusPressed = true;
    Serial.println("[COMBO] +/- detectado.");
    buttonPressedTimePlus  = 0;
    buttonPressedTimeMinus = 0;
    return;
  }

  bool pPlus  = manejarMasFluido();
  bool pMinus = manejarMenosFluido();

  switch (alarmState) {
    // Confirmar alarma anterior
    case STATE_CONFIRM_PREVIOUS:
      if (pPlus || pMinus) {
        tempHour   = alarmHour;
        tempMinute = alarmMinute;
        alarmState = STATE_SET_HOUR;
        encenderLedAzulFijo();
        reiniciarEstadoMenu();
        mostrarConfigurarHora(tempHour);
      }
      else if (seDetectoEnter()) {
        alarmEnabled = true;
        alarmFired   = false;
        tft.fillScreen(TFT_BLACK);
        alarmState = STATE_ACTIVE;
        alarmaRecienConfirmada = true;

        iniciarParpadeoVerde(2000);

        mostrarReloj(timeinfo, alarmHour, alarmMinute, alarmEnabled, oximetroHabilitadoEmisor, wifiEstaConectado(), conectadoBT);
      }
      break;

    // Configurar hora
    case STATE_SET_HOUR:
      if (pPlus) {
        tempHour = (tempHour + 1) % 24;
        mostrarConfigurarHora(tempHour);
      }
      else if (pMinus) {
        tempHour = (tempHour - 1 + 24) % 24;
        mostrarConfigurarHora(tempHour);
      }
      else if (seDetectoEnter()) {
        alarmHour = tempHour;
        reiniciarEstadoMenu();
        alarmState = STATE_SET_MINUTE;
        mostrarConfigurarMinutos(tempHour, tempMinute);
      }
      break;

    // Configurar minutos
    case STATE_SET_MINUTE:
      if (pPlus) {
        tempMinute = (tempMinute + 5) % 60;
        mostrarConfigurarMinutos(tempHour, tempMinute);
      }
      else if (pMinus) {
        tempMinute = (tempMinute - 5 + 60) % 60;
        mostrarConfigurarMinutos(tempHour, tempMinute);
      }
      else if (seDetectoEnter()) {
        alarmMinute  = tempMinute;
        alarmEnabled = true;
        alarmFired   = false;
        guardarAlarmaEnFlash();
        tft.fillScreen(TFT_BLACK);
        alarmState = STATE_ACTIVE;
        alarmaRecienConfirmada = true;

        iniciarParpadeoVerde(2000);

        mostrarReloj(timeinfo, alarmHour, alarmMinute, alarmEnabled, oximetroHabilitadoEmisor, wifiEstaConectado(), conectadoBT);
      }
      break;

    // Reloj activo
    case STATE_ACTIVE:
      if (!(timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute)) {
        alarmFired = false;
      }
      break;
  }
}

// Redibuja la pantalla actual (tras un aviso temporal que la tapo)
void redibujarAdministradorAlarma(struct tm timeinfo, bool oximetroActivo, bool wifiConectado, bool btConectado) {
  switch (alarmState) {
    case STATE_CONFIRM_PREVIOUS:
      mostrarConfirmarAnterior(alarmHour, alarmMinute);
      break;

    case STATE_SET_HOUR:
      reiniciarEstadoMenu();
      encenderLedAzulFijo();
      mostrarConfigurarHora(tempHour);
      break;

    case STATE_SET_MINUTE:
      reiniciarEstadoMenu();
      mostrarConfigurarMinutos(tempHour, tempMinute);
      break;

    case STATE_ACTIVE:
      UltimoMinuto = -1;   // fuerza el redibujado del reloj
      mostrarReloj(timeinfo, alarmHour, alarmMinute, alarmEnabled, oximetroActivo, wifiConectado, btConectado);
      break;
  }
}