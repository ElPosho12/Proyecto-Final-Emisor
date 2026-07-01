#pragma once

#include <Arduino.h>

// Pines de los pulsadores -------------------------------- 
#define BTN_PLUS   14   
#define BTN_MINUS  13   
#define BTN_ENTER  25 

//Estados de la máquina de estados ---------------------------- 
enum AlarmState {
  STATE_CONFIRM_PREVIOUS,   // ¿Usar alarma anterior?
  STATE_SET_HOUR,           // Configurar hora
  STATE_SET_MINUTE,         // Configurar minutos
  STATE_ACTIVE              // Alarma activa, mostrando reloj normal
};

//Variables globales accesibles desde main -------------------------
extern AlarmState alarmState;
extern int  alarmHour;
extern int  alarmMinute;
extern bool alarmFired;
extern bool alarmEnabled;

// Se pone en true cuando + y − se presionan juntos en STATE_ACTIVE.
// main.cpp del emisor la lee, envía '2' por BT al receptor, y la hace false.
extern bool comboPlusMinusPressed;

//Funciones públicas ----------------------------------------------
void alarmManagerInit();
void alarmManagerLoop(struct tm timeinfo);