#include "display_manager.h"
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

// ─── Pines TFT ────────────────────────────────────────────────────────────────
#define TFT_MOSI  4
#define TFT_CLK   5
#define TFT_CS    19
#define TFT_DC    15
#define TFT_RST   -1   // Conectado a 3.3V, no controlado por software

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);

// ─── Colores ──────────────────────────────────────────────────────────────────
#define COLOR_BG         ILI9341_BLACK
#define COLOR_TITLE      ILI9341_WHITE
#define COLOR_HOUR       ILI9341_GREEN
#define COLOR_ALARM      ILI9341_ORANGE
#define COLOR_BT_OK      ILI9341_CYAN
#define COLOR_BT_FAIL    ILI9341_RED
#define COLOR_HINT       0x7BEF   // Gris claro
#define COLOR_CONFIRM    ILI9341_YELLOW
#define COLOR_FIRED      ILI9341_RED
#define COLOR_BOX        0x1082   // Gris muy oscuro para el recuadro de la hora
#define COLOR_BOX_BORDER 0x4208   // Borde del recuadro

// Para evitar parpadeo, guardamos el último segundo dibujado
static int lastSecond = -1;

// ─── Init ─────────────────────────────────────────────────────────────────────
void displayInit() {
  tft.begin();
  tft.setRotation(1);   // Horizontal (320x240)
  tft.fillScreen(COLOR_BG);
}

// ─── Utilidad: línea divisoria ────────────────────────────────────────────────
static void drawDivider(int y) {
  tft.drawFastHLine(10, y, 300, COLOR_HINT);
}

// ─── Cabecera común ───────────────────────────────────────────────────────────
static void drawHeader(const char* subtitle) {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_TITLE);
  tft.setTextSize(2);
  tft.setCursor(10, 8);
  tft.println("DESPERTADOR SORDO");
  drawDivider(28);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_HINT);
  tft.setCursor(10, 34);
  tft.println(subtitle);
}

// ─── ¿Usar alarma anterior? ───────────────────────────────────────────────────
void displayConfirmPrevious(int hour, int minute) {
  drawHeader("Alarma guardada encontrada");

  tft.setTextColor(COLOR_CONFIRM);
  tft.setTextSize(3);
  tft.setCursor(60, 70);
  char buf[6];
  sprintf(buf, "%02d:%02d", hour, minute);
  tft.println(buf);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_HINT);
  tft.setCursor(10, 130);
  tft.println("Presiona ENTER para usar esta alarma");
  tft.setCursor(10, 148);
  tft.println("Presiona + o - para configurar nueva");
}

// ─── Configurar hora ──────────────────────────────────────────────────────────
void displaySetHour(int hour) {
  drawHeader("Paso 1 de 2: Configurar HORA");

  tft.setTextColor(COLOR_HOUR);
  tft.setTextSize(5);
  tft.setCursor(80, 65);
  char buf[3];
  sprintf(buf, "%02d", hour);
  tft.println(buf);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_HINT);
  tft.setCursor(10, 160);
  tft.println("  [ - ]  bajar    [ + ]  subir    [ ENTER ]  confirmar");
}

// ─── Configurar minutos ───────────────────────────────────────────────────────
void displaySetMinute(int minute) {
  drawHeader("Paso 2 de 2: Configurar MINUTOS");

  tft.setTextColor(COLOR_ALARM);
  tft.setTextSize(5);
  tft.setCursor(80, 65);
  char buf[3];
  sprintf(buf, "%02d", minute);
  tft.println(buf);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_HINT);
  tft.setCursor(10, 160);
  tft.println("  [ - ]  bajar    [ + ]  subir    [ ENTER ]  confirmar");
}

// ─── Reloj principal ──────────────────────────────────────────────────────────
// oximetroActivo: estado del MAX30102, recibido desde main.cpp del emisor
void displayClock(struct tm timeinfo, int alarmHour, int alarmMinute, bool alarmEnabled, bool oximetroActivo) {
  if (timeinfo.tm_sec == lastSecond) return;
  lastSecond = timeinfo.tm_sec;

  // ── Título ────────────────────────────────────────────────────────────────
  tft.fillRect(0, 0, 320, 38, COLOR_BG);
  tft.setTextColor(COLOR_TITLE);
  tft.setTextSize(2);
  tft.setCursor(10, 8);
  tft.println("DESPERTADOR SORDO");
  drawDivider(28);

  // ── Recuadro centrado con la hora ─────────────────────────────────────────
  // Recuadro: x=20, y=42, w=280, h=80
  tft.fillRect(20, 42, 280, 80, COLOR_BOX);
  tft.drawRect(20, 42, 280, 80, COLOR_BOX_BORDER);

  // TextSize 4 → cada carácter mide 24px de ancho
  // "HH:MM:SS" = 8 chars × 24px = 192px → centrado en 320px: x = (320-192)/2 = 64
  char timeBuf[9];
  sprintf(timeBuf, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  tft.setTextSize(4);
  tft.setTextColor(COLOR_HOUR);
  tft.setCursor(64, 66);
  tft.println(timeBuf);

  // ── Separador ─────────────────────────────────────────────────────────────
  drawDivider(130);

  // ── Alarma ────────────────────────────────────────────────────────────────
  tft.fillRect(0, 134, 320, 18, COLOR_BG);
  tft.setTextSize(1);
  if (alarmEnabled) {
    tft.setTextColor(COLOR_ALARM);
    tft.setCursor(10, 138);
    char alarmBuf[30];
    sprintf(alarmBuf, "Alarma: %02d:%02d hs", alarmHour, alarmMinute);
    tft.println(alarmBuf);
  } else {
    tft.setTextColor(COLOR_HINT);
    tft.setCursor(10, 138);
    tft.println("Sin alarma configurada");
  }

  // ── Estado MAX30102 ───────────────────────────────────────────────────────
  tft.fillRect(0, 156, 320, 18, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 160);
  if (oximetroActivo) {
    tft.setTextColor(COLOR_BT_OK);
    tft.println("MAX30102: Activo");
  } else {
    tft.setTextColor(COLOR_BT_FAIL);
    tft.println("MAX30102: Inactivo  (reconectar BT para reactivar)");
  }
}

// ─── Estado Bluetooth ─────────────────────────────────────────────────────────
void displaySetBTStatus(bool connected) {
  tft.fillRect(180, 178, 140, 12, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(180, 178);
  if (connected) {
    tft.setTextColor(COLOR_BT_OK);
    tft.println("BT: Conectado");
  } else {
    tft.setTextColor(COLOR_BT_FAIL);
    tft.println("BT: Buscando...");
  }
}

// ─── Alarma disparada ─────────────────────────────────────────────────────────
void displayAlarmFired() {
  tft.fillRect(0, 200, 320, 40, COLOR_FIRED);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(40, 212);
  tft.println("!ALARMA ENVIADA!");
}