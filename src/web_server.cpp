#include "web_server.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <Preferences.h>
#include <SPIFFS.h>

WebServer server(80);

// ─── Prototipos de funciones (Esto soluciona el error del Linker) ────────────
static void handleRoot();
static void handleSave();
static void handleStatus();
static void serveFile(const char* path, const char* contentType);

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

// ─── GET / → sirve index.html con placeholders reemplazados ────────────────
// ─── GET / → sirve index.html de forma segura y directa ────────────────
static void handleRoot() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    Serial.println("[Web] No se pudo abrir /index.html");
    server.send(404, "text/plain", "Archivo index.html no encontrado");
    return;
  }

  // Leemos todo el HTML de una sola vez de forma eficiente
  String html = file.readString();
  file.close();

  // Preparamos los valores según el estado del WiFi
  String dot, label, text, ip;
  if (wifiIsConnected()) {
    dot   = "dot-ok";
    label = "Conectado";
    text  = "Red: " + savedSSID;
    ip    = "IP: " + WiFi.localIP().toString();
  } else if (savedSSID != "") {
    dot   = "dot-warn";
    label = "Conectando...";
    text  = wifiFailReason != "" ? wifiFailReason : "Intentando conectar...";
    ip    = "";
  } else {
    dot   = "dot-error";
    label = "Desconectado";
    text  = "Ingresa el nombre y contrasena del WiFi";
    ip    = "";
  }

  // Hacemos todos los reemplazos de manera segura sobre el string completo
  html.replace("%SSID%",         savedSSID);
  html.replace("%DOT_CLASS%",    dot);
  html.replace("%STATUS_LABEL%", label);
  html.replace("%STATUS_TEXT%",  text);
  html.replace("%STATUS_IP%",    ip);
  html.replace("%FLASH%",        "");

  // Enviamos todo el bloque limpio al navegador
  server.send(200, "text/html", html);
  Serial.println("[Web] index.html procesado y servido correctamente.");
}

// ─── POST /save → guarda credenciales y reconecta ──────────────────────────
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

  // Respondemos un JSON de éxito para que la web mantenga la animación en vivo
  server.send(200, "application/json", "{\"status\":\"ok\"}");
  
  unsigned char i = 0;
  while(i < 10) {
    server.handleClient();
    delay(20);
    i++;
  }

  wifiConnect();
}

// ─── GET /status → JSON con el estado actual ───────────────────────────────
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

  // Listar archivos en SPIFFS
  Serial.println("[Web] Archivos en SPIFFS:");
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.printf("  - %s (%d bytes)\n", file.name(), file.size());
    file = root.openNextFile();
  }

  // Registro de rutas limpio usando Lambdas para evitar caídas en ráfagas de CSS/JS
  server.on("/", HTTP_GET, handleRoot);
  
  server.on("/style.css", HTTP_GET, []() {
    serveFile("/style.css", "text/css");
  });
  
  server.on("/app.js", HTTP_GET, []() {
    serveFile("/app.js", "application/javascript");
  });
  
  server.on("/save",   HTTP_POST, handleSave);
  server.on("/status", HTTP_GET,  handleStatus);

  server.begin();
  Serial.println("[Web] Servidor HTTP iniciado en puerto 80.");
}

// ─── Loop Principal del Servidor ──────────────────────────────────────────────
void webServerLoop() {
  server.handleClient();
}