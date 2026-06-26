#include "display_manager.h"
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

// ─── Pines TFT ────────────────────────────────────────────────────────────────
#define TFT_MOSI  4
#define TFT_CLK   5
#define TFT_CS    19
#define TFT_DC    15
#define TFT_RST   -1   

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);

// ─── Paleta de Colores de la Interfaz ─────────────────────────────────────────
#define COLOR_BG         ILI9341_BLACK
#define COLOR_TITLE      ILI9341_WHITE
#define COLOR_HINT       0x7BEF   
#define COLOR_DARK_GRAY  0x3186   
#define COLOR_BOX        0x1082   
#define COLOR_BOX_BORDER 0x4208   
#define COLOR_CELESTE    0x05FF   
#define COLOR_NARANJA    0xFC60   
#define COLOR_OK         ILI9341_GREEN
#define COLOR_FAIL       ILI9341_RED

// ─── Variables de Estado de Vistas ────────────────────────────────────────────
int lastMinute       = -1;
static int lastHourView     = -1;
static int lastMinuteView   = -1;

static void drawStatusIndicators(bool wifiConectado, bool btConectado, bool oximetroActivo) {
  int radius = 6;
  tft.setTextSize(2);
  
  // 1. Estado WiFi
  tft.setCursor(40, 140);
  if (wifiConectado) {
    tft.fillCircle(25, 143, radius, COLOR_OK);
    tft.setTextColor(COLOR_OK);
    tft.println("WIFI CONECTADO");
  } else {
    tft.fillCircle(25, 143, radius, COLOR_FAIL);
    tft.setTextColor(COLOR_FAIL);
    tft.println("WIFI DESCONECT.");
  }

  // 2. Estado Bluetooth
  tft.setCursor(40, 160);
  if (btConectado) {
    tft.fillCircle(25, 163, radius, COLOR_OK);
    tft.setTextColor(COLOR_OK);
    tft.println("RECEPTOR OK");
  } else {
    tft.fillCircle(25, 163, radius, COLOR_FAIL);
    tft.setTextColor(COLOR_FAIL);
    tft.println("RECEPTOR OFF");
  }

  // 3. Estado en tiempo real del oxímetro MAX30102
  tft.setCursor(40, 180);
  if (oximetroActivo) {
    tft.fillCircle(25, 183, radius, COLOR_OK);
    tft.setTextColor(COLOR_OK);
    tft.println("MAX30102 ACTIVO");
  } else {
    // 🚀 MODIFICADO: Ahora cambia a rojo y muestra tu texto personalizado en el emisor
    tft.fillCircle(25, 183, radius, COLOR_FAIL);
    tft.setTextColor(COLOR_FAIL);
    tft.println("MAX30102 DESACTIVADO");
  }
}

void displayInit() {
  tft.begin();
  tft.setRotation(3); // Horizontal panorámico
  tft.fillScreen(COLOR_BG);
}

void displayResetMenuState() {
  lastHourView   = -1;
  lastMinuteView = -1;
}

void displayConfirmPrevious(int hour, int minute) {
  tft.fillScreen(COLOR_BG);
  
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TITLE);
  tft.setCursor(20, 30);
  tft.println("¿Usar alarma anterior?");

  tft.fillRect(40, 70, 240, 60, COLOR_BOX);
  tft.drawRect(40, 70, 240, 60, COLOR_BOX_BORDER);

  tft.setTextSize(4);
  tft.setTextColor(COLOR_CELESTE);
  tft.setCursor(100, 83);
  char buf[6];
  sprintf(buf, "%02d:%02d", hour, minute);
  tft.print(buf);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_HINT);
  tft.setCursor(40, 150);
  tft.println("[ENTER] Confirmar vieja  /  [+] o [-] Crear nueva");
}

void displaySetHour(int hour) {
  if (lastHourView == hour) return;

  if (lastHourView == -1) {
    tft.fillScreen(COLOR_BG);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TITLE);
    tft.setCursor(20, 25);
    tft.println("Configurar Hora:");

    tft.fillRect(80, 60, 55, 40, COLOR_BOX); 
    tft.fillRect(145, 75, 10, 15, COLOR_BG);
    tft.fillRect(165, 60, 55, 40, COLOR_BOX);

    tft.setTextSize(4);
    tft.setTextColor(COLOR_DARK_GRAY);
    tft.setCursor(165, 60);
    tft.print("--");
    tft.setTextColor(COLOR_TITLE);
    tft.setCursor(145, 60);
    tft.print(":");
  }

  tft.fillRect(80, 60, 55, 40, COLOR_BOX);
  tft.setTextSize(5);
  tft.setTextColor(COLOR_CELESTE);
  tft.setCursor(80, 60);
  char bufH[3];
  sprintf(bufH, "%02d", hour);
  tft.print(bufH);

  lastHourView = hour;
}

void displaySetMinute(int hour, int minute) {
  if (lastMinuteView == minute) return;

  if (lastMinuteView == -1) {
    tft.fillRect(20, 25, 250, 25, COLOR_BG);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TITLE);
    tft.setCursor(20, 25);
    tft.println("Configurar Minutos:");

    tft.fillRect(80, 60, 55, 40, COLOR_BOX);
    tft.setTextSize(5);
    tft.setTextColor(COLOR_DARK_GRAY);
    tft.setCursor(80, 60);
    char bufH[3];
    sprintf(bufH, "%02d", hour);
    tft.print(bufH);

    tft.setTextSize(4);
    tft.setTextColor(COLOR_TITLE);
    tft.setCursor(145, 60);
    tft.print(":");
  }

  tft.fillRect(185, 60, 55, 40, COLOR_BOX); 
  tft.setTextSize(5);
  tft.setTextColor(COLOR_NARANJA);
  tft.setCursor(185, 60);
  char bufM[3];
  sprintf(bufM, "%02d", minute);
  tft.print(bufM);
  
  lastMinuteView = minute;
}

void displayClock(struct tm timeinfo, int alarmHour, int alarmMinute, bool alarmEnabled, bool oximetroActivo, bool wifiConectado, bool btConectado) {
  if (timeinfo.tm_min == lastMinute) return;
  lastMinute = timeinfo.tm_min;

  tft.fillScreen(COLOR_BG);

  tft.fillRect(20, 42, 280, 80, COLOR_BOX);
  tft.drawRect(20, 42, 280, 80, COLOR_BOX_BORDER);

  char timeBuf[6];
  sprintf(timeBuf, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  tft.setTextSize(5);
  tft.setTextColor(COLOR_CELESTE);
  tft.setCursor(85, 60);
  tft.print(timeBuf);

  tft.setTextSize(2);
  tft.setTextColor(COLOR_HINT);
  tft.setCursor(25, 15);
  tft.print("Alarma: ");
  
  if (alarmEnabled) {
    tft.setTextColor(COLOR_NARANJA);
    char alarmBuf[6];
    sprintf(alarmBuf, "%02d:%02d", alarmHour, alarmMinute);
    tft.print(alarmBuf);
  } else {
    tft.setTextColor(COLOR_DARK_GRAY);
    tft.print("OFF");
  }

  drawStatusIndicators(wifiConectado, btConectado, oximetroActivo);
}

void displayAlarmFired() {
  tft.fillScreen(COLOR_FAIL);
  tft.setTextSize(4);
  tft.setTextColor(COLOR_TITLE);
  tft.setCursor(45, 90);
  tft.print("¡DESPIERTA!");
}