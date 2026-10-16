#pragma once

#include <WebServer.h>

// Servidor accesible desde otros modulos
extern WebServer server;

void iniciarServidorWeb();
void actualizarServidorWeb();