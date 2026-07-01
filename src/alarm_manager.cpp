#include "alarm_manager.h"
#include "display_manager.h"
#include <Preferences.h>
#include "wifi_manager.h"
#include <Adafruit_ILI9341.h> 

// Traemos el objeto original tft
extern Adafruit_ILI9341 tft; 

// ─── Variables globales ───────────────────────────────────────────────────────
#define LED_ROJO   32
#define LED_VERDE  26
#define LED_AZUL   33

extern bool conectadoBT;
extern bool oximetroHabilitadoEmisor;
extern bool oximetroHabilitadoEmisor;
extern bool conectadoBT;
extern bool oximetroHabilitadoEmisor;

AlarmState alarmState   = STATE_CONFIRM_PREVIOUS;
int  alarmHour          = 7;
int  alarmMinute        = 0;
bool alarmFired         = false;
bool alarmEnabled       = false;

bool comboPlusMinusPressed = false;

// Valor temporal mientras el usuario configura
static int  tempHour    = 7;
static int  tempMinute  = 0;
static bool hasPrevious = false;   // si hay alarma guardada en flash

// ─── Variables para Aceleración y Fluidez de Pulsado (No Bloqueante) ─────────
static unsigned long buttonPressedTimePlus  = 0;
static unsigned long buttonPressedTimeMinus = 0;
static unsigned long lastActionTime         = 0;
static unsigned long lastPressEnter         = 0;

#define LONG_PRESS_DELAY  500   // ms para detectar que se mantiene apretado
#define REPEAT_INTERVAL   120   // ms de cambio al mantener apretado

// ─── Detección de Combo +/− simultáneos ──────────────────────────────────────
#define COMBO_HOLD_MS  150   // ms que deben mantenerse juntos para confirmar combo

static unsigned long comboStartTime  = 0;   
static bool          comboFired      = false; 

void encenderLedMomentaneo(uint8_t pin, unsigned long duracionMs);

static bool detectCombo() {
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

// ─── Control Fluido de Botón Más (+) ─────────────────────────────────────────
static bool handlePlusFluido() {
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

// ─── Control Fluido de Botón Menos (-) ───────────────────────────────────────
static bool handleMinusFluido() {
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

// ─── Antirebote para botón ENTER ─────────────────────────────────────────────
static bool pressedEnter() {
  if (digitalRead(BTN_ENTER) == LOW && millis() - lastPressEnter > 300) {
    lastPressEnter = millis();
    while (digitalRead(BTN_ENTER) == LOW);
    return true;
  }
  return false;
}

// ─── Guardar y Cargar desde Memoria Flash (NVS) ──────────────────────────────
static void saveAlarmToFlash() {
  Preferences prefs;
  prefs.begin("alarm", false);
  prefs.putInt("hour",    alarmHour);
  prefs.putInt("minute",  alarmMinute);
  prefs.putBool("exists", true);
  prefs.end();
}

static bool loadAlarmFromFlash() {
  Preferences prefs;
  prefs.begin("alarm", true);
  bool exists = prefs.getBool("exists", false);
  if (exists) {
    alarmHour   = prefs.getInt("hour",   7);
    alarmMinute = prefs.getInt("minute", 0);
  }
  prefs.end();
  return exists;
}

// ─── Inicialización del Administrador de Alarma ───────────────────────────────
void alarmManagerInit() {
  pinMode(BTN_PLUS,  INPUT_PULLUP);
  pinMode(BTN_MINUS, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);

  hasPrevious = loadAlarmFromFlash();

  if (hasPrevious) {
    alarmState = STATE_CONFIRM_PREVIOUS;
    displayConfirmPrevious(alarmHour, alarmMinute);
  } else {
    tempHour   = 7;
    tempMinute = 0;
    alarmState = STATE_SET_HOUR;
    displaySetHour(tempHour);
  }
}

// ─── Bucle Principal del Administrador de Alarma ─────────────────────────────
void alarmManagerLoop(struct tm timeinfo) {
  if (alarmState == STATE_ACTIVE && detectCombo()) {
    comboPlusMinusPressed = true;   
    Serial.println("[COMBO] +/- detectado: solicitando apagado de oxímetro.");
    buttonPressedTimePlus  = 0;
    buttonPressedTimeMinus = 0;
    return;   
  }

  bool pPlus  = handlePlusFluido();
  bool pMinus = handleMinusFluido();

  switch (alarmState) {

    // ── Pantalla de Confirmación de Alarma Anterior ───────────────────────────
    case STATE_CONFIRM_PREVIOUS:
      if (pPlus || pMinus) {
        tempHour   = alarmHour;
        tempMinute = alarmMinute;
        alarmState = STATE_SET_HOUR;
        displayResetMenuState();
        displaySetHour(tempHour);
      }
      else if (pressedEnter()) {
        alarmEnabled = true;
        alarmFired   = false;
        tft.fillScreen(ILI9341_BLACK);
        alarmState = STATE_ACTIVE;

        // 🚀 NUEVO: Prende en verde al confirmar alarma vieja
        encenderLedMomentaneo(LED_VERDE, 2000); 

        displayClock(timeinfo, alarmHour, alarmMinute, alarmEnabled, oximetroHabilitadoEmisor, wifiIsConnected(), conectadoBT);
      }
      break;

    // ── Configurar hora ───────────────────────────────────────────────────────
    case STATE_SET_HOUR:
      if (pPlus) {
        tempHour = (tempHour + 1) % 24;
        displaySetHour(tempHour);
      }
      else if (pMinus) {
        tempHour = (tempHour - 1 + 24) % 24;
        displaySetHour(tempHour);
      }
      else if (pressedEnter()) {
        alarmHour  = tempHour;
        
        // Se limpian los estados de pantalla para obligar el redibujado base de los minutos
        displayResetMenuState(); 
        
        alarmState = STATE_SET_MINUTE;
        displaySetMinute(tempHour, tempMinute);
      }
      break;

// ── Configurar minutos ────────────────────────────────────────────────────
    case STATE_SET_MINUTE:
      if (pPlus) {
        tempMinute = (tempMinute + 5) % 60;
        displaySetMinute(tempHour, tempMinute);
      }
      else if (pMinus) {
        tempMinute = (tempMinute - 5 + 60) % 60;
        displaySetMinute(tempHour, tempMinute);
      }
      else if (pressedEnter()) {
        alarmMinute  = tempMinute;
        alarmEnabled = true;
        alarmFired   = false;
        saveAlarmToFlash();
        tft.fillScreen(ILI9341_BLACK); 
        alarmState = STATE_ACTIVE;

        // 🚀 NUEVO: Prende en verde al terminar de configurar hora y minutos
        encenderLedMomentaneo(LED_VERDE, 2000); 

        displayClock(timeinfo, alarmHour, alarmMinute, alarmEnabled, oximetroHabilitadoEmisor, wifiIsConnected(), conectadoBT);
      }
      break;
    // ── Reloj activo ──────────────────────────────────────────────────────────
    case STATE_ACTIVE:
      if (!(timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute)) {
        alarmFired = false;
      }
      break;
  }
}