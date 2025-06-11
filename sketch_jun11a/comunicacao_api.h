#ifndef COMUNICACAO_API_H
#define COMUNICACAO_API_H

#include <Arduino.h>
#include "sensores.h" // Necessário para DadosSensores em montarPayloadJson
#include <ArduinoJson.h> // Necessário para JsonDocument em montarPayloadJson

void inicializarNTP();
// Modificado para aceitar um payload JSON completo e retornar status de envio
bool enviarDadosApi(const String& jsonPayload, const String& endpointApi, const String& tokenApi);
String obterTimestampFormatado();

// Declaração da função auxiliar para montar o JSON de dados dos sensores
String montarPayloadJson(const DadosSensores& dados, const String& deviceId);

// Novas declarações para diagnóstico remoto
String montarPayloadStatus(const String& deviceId, const String& ipAddress, int rssi, unsigned long uptimeSeconds, uint32_t freeHeap, int bufferedMessages, unsigned int wifiReconnects);
bool enviarDadosStatus(const String& payloadStatusJson, const String& endpointApiStatus, const String& tokenApi);

extern unsigned long ultimoEnvioStatus;

// Para polling de comandos
String buscarComandosRemotos(const String& deviceId, const String& endpointCmdBase, const String& tokenApi);
extern unsigned long ultimoPollingComandos;

#endif
