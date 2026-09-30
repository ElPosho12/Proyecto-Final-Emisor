#include "display_manager.h"
#include <TFT_eSPI.h>

// Pines de la TFT se configuran en platformio.ini / User_Setup.h
TFT_eSPI tft = TFT_eSPI();

// Paleta de colores
#define COLOR_BG         TFT_BLACK
#define COLOR_TITLE      TFT_WHITE
#define COLOR_HINT       0x7BEF
#define COLOR_DARK_GRAY  0x3186
#define COLOR_BOX        0x1082
#define COLOR_BOX_BORDER 0x4208
#define COLOR_CELESTE    0x05FF
#define COLOR_NARANJA    0xFC60
#define COLOR_OK         TFT_GREEN
#define COLOR_FAIL       TFT_BLACK

int UltimoMinuto            = -1;
static int lastHourView   = -1;
static int lastMinuteView = -1;

// Dibuja los tres indicadores de estado (WiFi, BT, oximetro)
static void dibujarIndicadoresEstado(bool wifiConectado, bool btConectado, bool oximetroActivo) {
  int radius = 6;
  tft.setTextSize(2);

  tft.setCursor(40, 140);
  if (wifiConectado) {
    tft.fillCircle(25, 143, radius, COLOR_OK);
    tft.setTextColor(COLOR_OK, COLOR_BG);
    tft.println("WIFI CONECTADO");
  } else {
    tft.fillCircle(25, 143, radius, COLOR_FAIL);
    tft.setTextColor(COLOR_FAIL, COLOR_BG);
    tft.println("WIFI DESCONECT.");
  }

  tft.setCursor(40, 160);
  if (btConectado) {
    tft.fillCircle(25, 163, radius, COLOR_OK);
    tft.setTextColor(COLOR_OK, COLOR_BG);
    tft.println("RECEPTOR OK");
  } else {
    tft.fillCircle(25, 163, radius, COLOR_FAIL);
    tft.setTextColor(COLOR_FAIL, COLOR_BG);
    tft.println("RECEPTOR OFF");
  }

  tft.setCursor(40, 180);
  if (oximetroActivo) {
    tft.fillCircle(25, 183, radius, COLOR_OK);
    tft.setTextColor(COLOR_OK, COLOR_BG);
    tft.println("MAX30102 ACTIVO");
  } else {
    tft.fillCircle(25, 183, radius, COLOR_FAIL);
    tft.setTextColor(COLOR_FAIL, COLOR_BG);
    tft.println("MAX30102 DESACTIVADO");
  }
}

void iniciarPantalla() {
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(COLOR_BG);
}

void reiniciarEstadoMenu() {
  lastHourView   = -1;
  lastMinuteView = -1;
}

// Pantalla: confirmar alarma anterior
void mostrarConfirmarAnterior(int hour, int minute) {
  tft.fillScreen(COLOR_BG);

  tft.setTextSize(2);
  tft.setTextColor(COLOR_TITLE, COLOR_BG);
  tft.setCursor(20, 30);
  tft.println("¿Usar alarma anterior?");

  tft.fillRect(40, 70, 240, 60, COLOR_BOX);
  tft.drawRect(40, 70, 240, 60, COLOR_BOX_BORDER);

  tft.setTextSize(4);
  tft.setTextColor(COLOR_CELESTE, COLOR_BOX);
  tft.setCursor(100, 83);
  char buf[6];
  sprintf(buf, "%02d:%02d", hour, minute);
  tft.print(buf);

  tft.setTextSize(2);
  tft.setTextColor(COLOR_HINT, COLOR_BG);
  tft.setCursor(15, 150);
  tft.println("[ENTER]");
  tft.setCursor(10, 170);
  tft.println("Confirmar //");
  tft.setCursor(150, 150);
  tft.println("// [+] O [-]");
  tft.setCursor(130, 170);
  tft.println("// Crear nueva");
}

// Pantalla: configurar hora
void mostrarConfigurarHora(int hour) {
  if (lastHourView == hour) return;

  if (lastHourView == -1) {
    tft.fillScreen(COLOR_BG);

    tft.fillRoundRect(15, 15, 290, 210, 10, COLOR_BOX);
    tft.drawRoundRect(15, 15, 290, 210, 10, COLOR_BOX_BORDER);

    tft.setTextSize(2);
    tft.setTextColor(COLOR_TITLE, COLOR_BOX);
    tft.setCursor(35, 30);
    tft.println("Configurar Alarma");

    tft.setTextSize(1);
    tft.setTextColor(COLOR_CELESTE, COLOR_BOX);
    tft.setCursor(35, 55);
    tft.println("PASO 1/2: Selecciona la Hora");

    tft.setTextSize(5);
    tft.setTextColor(COLOR_TITLE, COLOR_BOX);
    tft.setCursor(152, 95);
    tft.print(":");

    tft.fillRoundRect(175, 90, 85, 60, 8, COLOR_BG);
    tft.drawRoundRect(175, 90, 85, 60, 8, COLOR_BOX_BORDER);
    tft.setTextSize(5);
    tft.setTextColor(COLOR_DARK_GRAY, COLOR_BG);
    tft.setCursor(190, 100);
    tft.print("--");

    tft.fillRoundRect(25, 175, 270, 30, 6, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_HINT, COLOR_BG);
    tft.setCursor(35, 186);
    tft.print("[+] / [-] Ajustar   [ENTER] Siguiente");
  }

  tft.fillRoundRect(60, 90, 85, 60, 8, COLOR_BG);
  tft.drawRoundRect(60, 90, 85, 60, 8, COLOR_CELESTE);

  tft.setTextSize(5);
  tft.setTextColor(COLOR_CELESTE, COLOR_BG);
  tft.setCursor(75, 100);
  char bufH[3];
  sprintf(bufH, "%02d", hour);
  tft.print(bufH);

  lastHourView = hour;
}

// Pantalla: configurar minutos
void mostrarConfigurarMinutos(int hour, int minute) {
  if (lastMinuteView == minute) return;

  if (lastMinuteView == -1) {
    tft.fillScreen(COLOR_BG);

    tft.fillRoundRect(15, 15, 290, 210, 10, COLOR_BOX);
    tft.drawRoundRect(15, 15, 290, 210, 10, COLOR_BOX_BORDER);

    tft.setTextSize(2);
    tft.setTextColor(COLOR_TITLE, COLOR_BOX);
    tft.setCursor(35, 30);
    tft.println("Configurar Alarma");

    tft.setTextSize(1);
    tft.setTextColor(COLOR_NARANJA, COLOR_BOX);
    tft.setCursor(35, 55);
    tft.println("PASO 2/2: Selecciona los Minutos");

    tft.setTextSize(5);
    tft.setTextColor(COLOR_TITLE, COLOR_BOX);
    tft.setCursor(152, 95);
    tft.print(":");

    tft.fillRoundRect(60, 90, 85, 60, 8, COLOR_BG);
    tft.drawRoundRect(60, 90, 85, 60, 8, COLOR_DARK_GRAY);
    tft.setTextSize(5);
    tft.setTextColor(COLOR_DARK_GRAY, COLOR_BG);
    tft.setCursor(75, 100);
    char bufH[3];
    sprintf(bufH, "%02d", hour);
    tft.print(bufH);

    tft.fillRoundRect(25, 175, 270, 30, 6, COLOR_BG);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_HINT, COLOR_BG);
    tft.setCursor(35, 186);
    tft.print("[+] / [-] Ajustar   [ENTER] Guardar");
  }

  tft.fillRoundRect(175, 90, 85, 60, 8, COLOR_BG);
  tft.drawRoundRect(175, 90, 85, 60, 8, COLOR_NARANJA);

  tft.setTextSize(5);
  tft.setTextColor(COLOR_NARANJA, COLOR_BG);
  tft.setCursor(190, 100);
  char bufM[3];
  sprintf(bufM, "%02d", minute);
  tft.print(bufM);

  lastMinuteView = minute;
}

// Pantalla: reloj normal
void mostrarReloj(struct tm timeinfo, int alarmHour, int alarmMinute, bool alarmEnabled, bool oximetroActivo, bool wifiConectado, bool btConectado) {
  if (timeinfo.tm_min == UltimoMinuto) return;
  UltimoMinuto = timeinfo.tm_min;

  tft.fillScreen(COLOR_BG);

  tft.fillRect(20, 42, 280, 80, COLOR_BOX);
  tft.drawRect(20, 42, 280, 80, COLOR_BOX_BORDER);

  char timeBuf[6];
  sprintf(timeBuf, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  tft.setTextSize(5);
  tft.setTextColor(COLOR_CELESTE, COLOR_BOX);
  tft.setCursor(85, 60);
  tft.print(timeBuf);

  tft.setTextSize(2);
  tft.setTextColor(COLOR_HINT, COLOR_BG);
  tft.setCursor(25, 15);
  tft.print("Alarma: ");

  if (alarmEnabled) {
    tft.setTextColor(COLOR_NARANJA, COLOR_BG);
    char alarmBuf[6];
    sprintf(alarmBuf, "%02d:%02d", alarmHour, alarmMinute);
    tft.print(alarmBuf);
  } else {
    tft.setTextColor(COLOR_DARK_GRAY, COLOR_BG);
    tft.print("OFF");
  }

  dibujarIndicadoresEstado(wifiConectado, btConectado, oximetroActivo);
}

// Pantalla: WiFi conectado
void mostrarWifiConectado() {
  tft.fillScreen(COLOR_BG);

  tft.fillRoundRect(20, 30, 280, 180, 10, COLOR_BOX);
  tft.drawRoundRect(20, 30, 280, 180, 10, COLOR_CELESTE);

  tft.setTextSize(3);
  tft.setTextColor(COLOR_CELESTE, COLOR_BOX);
  tft.setCursor(95, 55);
  tft.print("Wi-Fi");

  tft.setTextSize(2);
  tft.setTextColor(COLOR_OK, COLOR_BOX);
  tft.setCursor(80, 105);
  tft.print("CONECTADO");

  tft.fillRoundRect(50, 145, 220, 6, 3, COLOR_OK);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_HINT, COLOR_BOX);
  tft.setCursor(85, 170);
  tft.print("Red sincronizada");
}

// Pantalla: Bluetooth conectado
void mostrarBtConectado() {
  tft.fillScreen(COLOR_BG);

  tft.fillRoundRect(20, 30, 280, 180, 10, COLOR_BOX);
  tft.drawRoundRect(20, 30, 280, 180, 10, COLOR_NARANJA);

  tft.setTextSize(3);
  tft.setTextColor(COLOR_NARANJA, COLOR_BOX);
  tft.setCursor(65, 55);
  tft.print("BLUETOOTH");

  tft.setTextSize(2);
  tft.setTextColor(COLOR_OK, COLOR_BOX);
  tft.setCursor(70, 105);
  tft.print("PULSERA OK");

  tft.fillRoundRect(50, 145, 220, 6, 3, COLOR_OK);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_HINT, COLOR_BOX);
  tft.setCursor(80, 170);
  tft.print("Receptor Vinculado");
}

// Pantalla: instrucciones iniciales de WiFi
void mostrarInstruccionesWifi() {
  tft.fillScreen(COLOR_BG);

  tft.fillRoundRect(10, 10, 300, 220, 10, COLOR_BOX);
  tft.drawRoundRect(10, 10, 300, 220, 10, COLOR_CELESTE);

  tft.setTextSize(2);
  tft.setTextColor(COLOR_TITLE, COLOR_BOX);
  tft.setCursor(28, 22);
  tft.println("Configura tu WiFi");

  tft.drawFastHLine(25, 48, 270, COLOR_BOX_BORDER);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_CELESTE, COLOR_BOX);
  tft.setCursor(25, 60);
  tft.println("PASO 1: Conectate al wifi 'Despertador-config'");
  tft.setCursor(25, 70);
  tft.println("e ingresa a la pagina");
  tft.setTextSize(2);
  tft.setTextColor(COLOR_OK, COLOR_BOX);
  tft.setCursor(25, 90);
  tft.println("http://192.168.4.1");

  tft.setTextSize(1);
  tft.setTextColor(COLOR_NARANJA, COLOR_BOX);
  tft.setCursor(25, 112);
  tft.println("PASO 2: Ingresa el nombre y");
  tft.setCursor(25, 124);
  tft.println("contrasena de tu WiFi");

  tft.drawFastHLine(25, 152, 270, COLOR_BOX_BORDER);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_HINT, COLOR_BOX);
  tft.setCursor(25, 166);
  tft.println("En caso de querer cambiar de WiFi,");
  tft.setCursor(25, 178);
  tft.println("volve a entrar a la pagina e");
  tft.setCursor(25, 190);
  tft.println("ingresalo de nuevo.");
}

// Franja inferior: "Buscando WiFi/Bluetooth"
#define ESTADO_ALTO_LINEA  14

static int alturaFranjaEstado() {
  return tft.height() - (2 * ESTADO_ALTO_LINEA);
}

static void dibujarLineaEstado(int y, bool conectado, const char* texto) {
  tft.fillRect(0, y, tft.width(), ESTADO_ALTO_LINEA, COLOR_BG);
  if (!conectado) {
    tft.setTextSize(1);
    tft.setTextColor(COLOR_HINT, COLOR_BG);
    tft.setCursor(6, y + 3);
    tft.print(texto);
  }
}

void mostrarEstadoBusqueda(bool wifiConectado, bool btConectado) {
  int yBase = alturaFranjaEstado();
  dibujarLineaEstado(yBase,                     wifiConectado, "Buscando WiFi...");
  dibujarLineaEstado(yBase + ESTADO_ALTO_LINEA, btConectado,   "Buscando Bluetooth...");
}

// Pantalla: alarma sonando
void mostrarAlarmaSonando() {
  tft.fillScreen(COLOR_FAIL);
  tft.setTextSize(4);
  tft.setTextColor(COLOR_TITLE, COLOR_FAIL);
  tft.setCursor(40, 90);
  tft.print("DESPERTATE");
}