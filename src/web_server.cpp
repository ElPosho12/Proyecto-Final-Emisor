#include "web_server.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <Preferences.h>
#include <SPIFFS.h>

WebServer server(80);

// ─── Utilidad: leer archivo de SPIFFS como String ────────────────────────────
static String readFile(const char* path) {
  File f = SPIFFS.open(path, "r");
  if (!f) {
    Serial.printf("[Web] No se pudo abrir %s\n", path);
    return "";
  }
  String content = f.readString();
  f.close();
  return content;
}

// ─── Utilidad: servir archivo de SPIFFS con content-type correcto ─────────────
static void serveFile(const char* path, const char* contentType) {
  File f = SPIFFS.open(path, "r");
  if (!f) {
    Serial.printf("[Web] No se pudo abrir %s\n", path);
    server.send(404, "text/plain", String("Archivo no encontrado: ") + path);
    return;
  }
  Serial.printf("[Web] Sirviendo %s (%d bytes)\n", path, f.size());
  server.streamFile(f, contentType);
  f.close();
}

// ─── GET /  →  sirve index.html con placeholders reemplazados ────────────────
// ─── GET /  →  Sirve index.html procesando placeholders al vuelo ────────────────
static void handleRoot() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    Serial.println("[Web] No se pudo abrir /index.html");
    server.send(404, "text/plain", "Archivo index.html no encontrado");
    return;
  }

  // Preparamos las variables de reemplazo para no procesarlas dentro del bucle
  String dot, label, text, ip;
  if (wifiIsConnected()) {
    dot   = "dot-ok";
    label = "Conectado";
    text  = "Red: " + savedSSID;
    ip    = "IP: " + WiFi.localIP().toString();
  } else if (savedSSID != "") {
    dot   = "dot-warn";
    label = "Sin conexion";
    text  = wifiFailReason != "" ? wifiFailReason : "Intentando conectar...";
    ip    = "";
  } else {
    dot   = "dot-error";
    label = "Sin configurar";
    text  = "Ingresa el nombre y contrasena del WiFi";
    ip    = "";
  }

  // Iniciamos una respuesta HTTP 200 de tipo HTML pero con tamaño desconocido (Chunked)
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  // Leemos y enviamos el archivo en pequeños fragmentos
  String line = "";
  while (file.available()) {
    char c = file.read();
    line += c;

    // Si encontramos un salto de línea o el buffer crece, procesamos los placeholders
    if (c == '\n' || line.length() > 128) {
      line.replace("%SSID%",         savedSSID);
      line.replace("%DOT_CLASS%",    dot);
      line.replace("%STATUS_LABEL%", label);
      line.replace("%STATUS_TEXT%",  text);
      line.replace("%STATUS_IP%",    ip);
      line.replace("%FLASH%",        "");

      server.sendContent(line); // Envía el fragmento al navegador inmediatamente
      line = "";                // Vacía el buffer para liberar la RAM
    }
  }
  
  // Enviamos lo último que haya quedado en el buffer si no terminó en \n
  if (line.length() > 0) {
    line.replace("%SSID%",         savedSSID);
    line.replace("%DOT_CLASS%",    dot);
    line.replace("%STATUS_LABEL%", label);
    line.replace("%STATUS_TEXT%",  text);
    line.replace("%STATUS_IP%",    ip);
    line.replace("%FLASH%",        "");
    server.sendContent(line);
  }

  // Enviamos un string vacío para avisarle al navegador que terminamos de transmitir
  server.sendContent(""); 
  file.close();
  Serial.println("[Web] index.html servido fluidamente.");
}

// ─── GET /style.css ───────────────────────────────────────────────────────────
static void handleCSS() {
  serveFile("/style.css", "text/css");
}

// ─── GET /app.js ──────────────────────────────────────────────────────────────
static void handleJS() {
  serveFile("/app.js", "application/javascript");
}

// ─── POST /save  →  guarda credenciales y reconecta ──────────────────────────

static void handleSave() {
  if (!server.hasArg("ssid") || server.arg("ssid") == "") {
    server.send(400, "text/plain", "Falta el nombre de red.");
    return;
  }

  savedSSID     = server.arg("ssid");
  savedPassword = server.arg("password");

  Preferences prefs;
  prefs.begin("wificfg", false);
  prefs.putString("ssid",     savedSSID);
  prefs.putString("password", savedPassword);
  prefs.end();

  // Respondemos algo super liviano para avisar que se recibió la info
  server.send(200, "text/plain", "OK. Conectando a la red, verifica el estado en unos segundos...");
  
  // Le damos un momento al servidor para que despache el paquete HTTP a la red física
  unsigned char i = 0;
  while(i < 10) {
    server.handleClient();
    delay(20);
    i++;
  }

  // Ahora sí, llamamos a la conexión
  wifiConnect();
}

// ─── GET /status  →  JSON con el estado actual ───────────────────────────────
static void handleStatus() {
  bool connected = wifiIsConnected();
  String json = "{";
  json += "\"connected\":"  + String(connected ? "true" : "false") + ",";
  json += "\"ssid\":\""     + savedSSID + "\",";
  json += "\"ip\":\""       + (connected ? WiFi.localIP().toString() : "") + "\",";
  json += "\"reason\":\""   + wifiFailReason + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// ─── Init ─────────────────────────────────────────────────────────────────────
void webServerInit() {
  if (!SPIFFS.begin(true)) {
    Serial.println("[Web] Error: no se pudo montar SPIFFS.");
    return;
  }
  Serial.println("[Web] SPIFFS montado.");

  // Listar archivos en SPIFFS para verificar que están
  Serial.println("[Web] Archivos en SPIFFS:");
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.printf("  - %s (%d bytes)\n", file.name(), file.size());
    file = root.openNextFile();
  }

  server.on("/",          HTTP_GET,  handleRoot);
  server.on("/style.css", HTTP_GET,  handleCSS);
  server.on("/app.js",    HTTP_GET,  handleJS);
  server.on("/save",      HTTP_POST, handleSave);
  server.on("/status",    HTTP_GET,  handleStatus);

  server.begin();
  Serial.println("[Web] Servidor HTTP iniciado en puerto 80.");
}

void webServerLoop() {
  server.handleClient();
}

