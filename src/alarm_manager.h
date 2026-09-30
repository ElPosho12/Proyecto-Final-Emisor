#pragma once

#include <Arduino.h>

// Pines de los pulsadores
#define BTN_PLUS   14
#define BTN_MINUS  13
#define BTN_ENTER  25

// Estados de la maquina de estados
enum AlarmState {
  STATE_CONFIRM_PREVIOUS,   // ¿Usar alarma anterior?
  STATE_SET_HOUR,           // Configurar hora
  STATE_SET_MINUTE,         // Configurar minutos
  STATE_ACTIVE              // Alarma activa, reloj normal
};

// Variables accesibles desde main
extern AlarmState alarmState;
extern int  alarmHour;
extern int  alarmMinute;
extern bool alarmFired;
extern bool alarmEnabled;

// true si se presiono + y - juntos en STATE_ACTIVE
extern bool comboPlusMinusPressed;

// true justo al confirmar la alarma y pasar a STATE_ACTIVE
extern bool alarmaRecienConfirmada;

void iniciarAdministradorAlarma();
void actualizarAdministradorAlarma(struct tm timeinfo);

// Redibuja la pantalla del estado actual (para volver tras un aviso temporal)
void redibujarAdministradorAlarma(struct tm timeinfo, bool oximetroActivo, bool wifiConectado, bool btConectado);