#ifndef CONFIGURACAO_H
#define CONFIGURACAO_H

#include <Arduino.h>

void carregarCredenciais(String& ssid, String& password);
void salvarCredenciais(const String& ssid, const String& password);
String obterDeviceId();
String obterEndpointApi();
String obterTokenApi();
void iniciarSpiffs();
String obterEndpointApiStatus(); // Nova função

// Constante para o novo endpoint
extern const char* ENDPOINT_API_STATUS;
extern const char* ENDPOINT_API_COMANDOS_BASE; // Novo endpoint base para comandos

#endif
