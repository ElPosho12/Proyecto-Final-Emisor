#pragma once

#include <WebServer.h>

// Servidor accesible desde main si hace falta
extern WebServer server;

void webServerInit();
void webServerLoop();
