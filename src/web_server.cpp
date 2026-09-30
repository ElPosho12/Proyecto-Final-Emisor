#include "web_server.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <Preferences.h>
#include <SPIFFS.h>

WebServer server(80);

static void manejarRaiz();
static void manejarGuardar();
static void manejarEstado();
static void servirArchivo(const char* path, const char* contentType);

// Lee un archivo de SPIFFS como String
static String leerArchivo(const char* path) {
  File f = SPIFFS.open(path, "r");
  if (!f) {
    Serial.printf("[Web] No se pudo abrir %s\n", path);
    return "";
  }
  String content = f.readString();
  f.close();
  return content;
}

// Sirve un archivo de SPIFFS con su content-type
static void servirArchivo(const char* path, const char* contentType) {
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

// GET / -> sirve index.html
static void manejarRaiz() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    Serial.println("[Web] No se pudo abrir /index.html");
    server.send(404, "text/plain", "Archivo index.html no encontrado");
    return;
  }

  String html = file.readString();
  file.close();

  // Arma los valores segun el estado del WiFi
  String dot, label, text, ip;
  if (wifiEstaConectado()) {
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

  html.replace("%SSID%",         savedSSID);
  html.replace("%DOT_CLASS%",    dot);
  html.replace("%STATUS_LABEL%", label);
  html.replace("%STATUS_TEXT%",  text);
  html.replace("%STATUS_IP%",    ip);
  html.replace("%FLASH%",        "");

  server.send(200, "text/html", html);
  Serial.println("[Web] index.html servido.");
}

// Guarda credenciales nuevas y reconecta
static void manejarGuardar() {
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

  server.send(200, "application/json", "{\"status\":\"ok\"}");

  unsigned char i = 0;
  while (i < 10) {
    server.handleClient();
    delay(20);
    i++;
  }

  conectarWifi();
}

// Devuelve el estado actual en JSON
static void manejarEstado() {
  bool connected = wifiEstaConectado();
  String json = "{";
  json += "\"connected\":"  + String(connected ? "true" : "false") + ",";
  json += "\"ssid\":\""     + savedSSID + "\",";
  json += "\"ip\":\""       + (connected ? WiFi.localIP().toString() : "") + "\",";
  json += "\"reason\":\""   + wifiFailReason + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// Inicia el servidor y registra las rutas
void iniciarServidorWeb() {
  if (!SPIFFS.begin(true)) {
    Serial.println("[Web] Error: no se pudo montar SPIFFS.");
    return;
  }
  Serial.println("[Web] SPIFFS montado.");

  Serial.println("[Web] Archivos en SPIFFS:");
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.printf("  - %s (%d bytes)\n", file.name(), file.size());
    file = root.openNextFile();
  }

  server.on("/", HTTP_GET, manejarRaiz);

  server.on("/style.css", HTTP_GET, []() {
    servirArchivo("/style.css", "text/css");
  });

  server.on("/app.js", HTTP_GET, []() {
    servirArchivo("/app.js", "application/javascript");
  });

  server.on("/save",   HTTP_POST, manejarGuardar);
  server.on("/status", HTTP_GET,  manejarEstado);

  server.begin();
  Serial.println("[Web] Servidor HTTP iniciado en puerto 80.");
}

// Atiende clientes conectados
void actualizarServidorWeb() {
  server.handleClient();
}