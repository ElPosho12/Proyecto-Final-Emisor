#include "display_manager.h"
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

// ─── Pines TFT ────────────────────────────────────────────────────────────────
#define TFT_MOSI  4
#define TFT_CLK   5
#define TFT_CS    19
#define TFT_DC    15
#define TFT_RST   -1   // Conectado a 3.3V de forma externa

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);

// ─── Paleta de Colores de la Interfaz ─────────────────────────────────────────
#define COLOR_BG         ILI9341_BLACK
#define COLOR_TITLE      ILI9341_WHITE
#define COLOR_HINT       0x7BEF   // Gris claro para textos secundarios
#define COLOR_DARK_GRAY  0x3186   // Gris oscuro para números inactivos o "--"
#define COLOR_BOX        0x1082   // Gris profundo para el fondo del reloj
#define COLOR_BOX_BORDER 0x4208   // Borde sutil del recuadro del reloj
#define COLOR_CELESTE    0x05FF   // Números de la hora en el reloj y menú
#define COLOR_NARANJA    0xFC60   // Configuración de minutos y alertas
#define COLOR_OK         ILI9341_GREEN
#define COLOR_FAIL       ILI9341_RED

// ─── Variables de Control de Estado (Evitan parpadeos) ────────────────────────
static int lastMinute = -1;
static int lastHourView = -1;
static int lastMinuteView = -1;
static bool menuIniciado = false;

// Llámase desde alarm_manager.cpp al cambiar de pantalla o salir al reloj
void displayResetMenuState() {
  menuIniciado = false;
  lastHourView = -1;
  lastMinuteView = -1;
  lastMinute = -1; // Fuerza el redibujado completo del reloj principal
}

// ─── Inicialización de Pantalla ───────────────────────────────────────────────
void displayInit() {
  tft.begin();
  tft.setRotation(1);   // Modo horizontal (320x240)
  tft.fillScreen(COLOR_BG);
}

// ─── Utilidad: Línea Divisoria Estética ───────────────────────────────────────
static void drawDivider(int y) {
  tft.drawFastHLine(10, y, 300, COLOR_HINT);
}

// ─── Cabecera Común de Menús Fijos ────────────────────────────────────────────
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

// ─── Indicadores Circulares Inferiores de Red y Sensores ──────────────────────
static void drawStatusIndicators(bool wifiConectado, bool btConectado, bool oximetroActivo) {
  int radius = 6;
  tft.setTextSize(1);
  
  // 1. Estado del Receptor (Bluetooth)
  tft.setCursor(40, 150);
  if (btConectado) {
    tft.fillCircle(25, 153, radius, COLOR_OK);
    tft.setTextColor(COLOR_OK);
    tft.println("RECEPTOR CONECTADO");
  } else {
    tft.fillCircle(25, 153, radius, COLOR_FAIL);
    tft.setTextColor(COLOR_FAIL);
    tft.println("BUSCANDO RECEPTOR...");
  }

  // 2. Estado de la conexión WiFi
  tft.setCursor(210, 150);
  if (wifiConectado) {
    tft.fillCircle(195, 153, radius, COLOR_OK);
    tft.setTextColor(COLOR_OK);
    tft.println("WIFI OK");
  } else {
    tft.fillCircle(195, 153, radius, COLOR_FAIL);
    tft.setTextColor(COLOR_FAIL);
    tft.println("BUSCANDO WIFI");
  }

  // 3. Estado en tiempo real del oxímetro MAX30102
  tft.setCursor(40, 180);
  if (oximetroActivo) {
    tft.fillCircle(25, 183, radius, COLOR_OK);
    tft.setTextColor(COLOR_OK);
    tft.println("MAX30102 ACTIVO");
  } else {
    tft.fillCircle(25, 183, radius, COLOR_FAIL);
    tft.setTextColor(COLOR_FAIL);
    tft.println("MAX30102 DESCONECTADO");
  }
}

// ─── Pantalla Inicial: ¿Usar Alarma Anterior? ───────────────────────────────────
void displayConfirmPrevious(int hour, int minute) {
  drawHeader("Alarma guardada encontrada");
  
  tft.setTextColor(COLOR_NARANJA);
  tft.setTextSize(3);
  tft.setCursor(60, 70);
  char buf[6];
  sprintf(buf, "%02d:%02d", hour, minute);
  tft.println(buf);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_HINT);
  tft.setCursor(10, 115);
  tft.println("[ENTER] Usar anterior   [ + / - ] Nueva alarma (HORA / MINUTOS)");

  // En el arranque frío del menú asumimos estados por defecto
  drawStatusIndicators(false, false, false);
}

// ─── Paso 1: Configurar HORA (Refresco Parcial Dinámico) ───────────────────────
void displaySetHour(int hour) {
  if (!menuIniciado) {
    tft.fillScreen(COLOR_BG);
    
    // Contenedor del reloj
    tft.fillRect(20, 42, 280, 80, COLOR_BOX);
    tft.drawRect(20, 42, 280, 80, COLOR_BOX_BORDER);

    // Separadores fijos
    tft.setTextSize(5);
    tft.setTextColor(COLOR_DARK_GRAY);
    tft.setCursor(135, 60); 
    tft.print(":");
    tft.print("--"); // Minutos apagados temporalmente

    // Subrayado interactivo de la hora
    tft.fillRect(75, 105, 55, 4, COLOR_CELESTE);

    // Guía de botones
    tft.setTextSize(1);
    tft.setTextColor(COLOR_HINT);
    tft.setCursor(10, 130);
    tft.println(" [ - ] Bajar   [ + ] Subir   [ ENTER ] Siguiente");

    drawStatusIndicators(false, false, false);
    menuIniciado = true;
  }

  // Sobreecritura parcial: cambia el número sin parpadear el fondo
  if (hour != lastHourView) {
    tft.fillRect(75, 60, 55, 40, COLOR_BOX); 
    
    tft.setTextSize(5);
    tft.setTextColor(COLOR_CELESTE);
    tft.setCursor(75, 60);
    char bufH[3];
    sprintf(bufH, "%02d", hour);
    tft.print(bufH);
    
    lastHourView = hour;
  }
}

// ─── Paso 2: Configurar MINUTOS (Refresco Parcial Dinámico) ────────────────────
// ─── Paso 2: Configurar MINUTOS (Refresco Parcial Dinámico) ────────────────────
// ─── Paso 2: Configurar MINUTOS (Refresco Parcial Dinámico) ────────────────────
void displaySetMinute(int hour, int minute) {
  // Forzamos el rediseño de la estructura si venimos del paso de la hora
  // (Detectamos que cambiamos de fase porque reseteamos lastMinuteView)
  if (lastMinuteView == -1) {
    menuIniciado = false; 
  }

  if (!menuIniciado) {
    tft.fillScreen(COLOR_BG);
    
    tft.fillRect(20, 42, 280, 80, COLOR_BOX);
    tft.drawRect(20, 42, 280, 80, COLOR_BOX_BORDER);

    // 1. Dibujar la HORA ya confirmada en GRIS OSCURO
    tft.setTextSize(5);
    tft.setTextColor(COLOR_DARK_GRAY);
    tft.setCursor(75, 60);
    char bufH[4];
    sprintf(bufH, "%02d:", hour);
    tft.print(bufH); 

    // 2. Subrayado interactivo NARANJA para los minutos
    tft.fillRect(185, 105, 55, 4, COLOR_NARANJA);

    // Guía de botones
    tft.setTextSize(1);
    tft.setTextColor(COLOR_HINT);
    tft.setCursor(10, 130);
    tft.println(" [ - ] Bajar   [ + ] Subir   [ ENTER ] Guardar");

    drawStatusIndicators(false, false, false);
    menuIniciado = true;
  }

  // Sobreecritura parcial dinámica de los dígitos de los minutos en NARANJA
  if (minute != lastMinuteView) {
    tft.fillRect(185, 60, 55, 40, COLOR_BOX); 
    
    tft.setTextSize(5);
    tft.setTextColor(COLOR_NARANJA);
    tft.setCursor(185, 60);
    char bufM[3];
    sprintf(bufM, "%02d", minute);
    tft.print(bufM);
    
    lastMinuteView = minute;
  }
}

// ─── Pantalla Principal: Reloj Central e Indicadores en Vivo ───────────────────
void displayClock(struct tm timeinfo, int alarmHour, int alarmMinute, bool alarmEnabled, bool oximetroActivo, bool wifiConectado, bool btConectado) {
  // Evita refrescar si no cambió el minuto (optimiza el bus SPI y elimina parpadeos)
  if (timeinfo.tm_min == lastMinute) return;
  lastMinute = timeinfo.tm_min;

  tft.fillScreen(COLOR_BG);

  // Recuadro Estilo Tablero del Reloj
  tft.fillRect(20, 42, 280, 80, COLOR_BOX);
  tft.drawRect(20, 42, 280, 80, COLOR_BOX_BORDER);

  // Hora central limpia en formato HH:MM (Color Celeste)
  char timeBuf[6];
  sprintf(timeBuf, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  tft.setTextSize(5);
  tft.setTextColor(COLOR_CELESTE);
  tft.setCursor(90, 60);
  tft.println(timeBuf);

  // Dibuja la barra inferior con los datos en tiempo real procesados por main.cpp
  drawStatusIndicators(wifiConectado, btConectado, oximetroActivo);
}

// ─── Aviso Crítico: Alarma Disparada hacia el Receptor ─────────────────────────
void displayAlarmFired() {
  tft.fillRect(0, 205, 320, 35, COLOR_FAIL);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(60, 215);
  tft.println("!ALARMA ACTIVADA!");
} 