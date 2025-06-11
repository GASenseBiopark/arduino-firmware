#include "configuracao.h"
#include <FS.h>
#include <ESP8266WiFi.h>

const char* ENDPOINT_API = "http://15.229.0.216:8080/api/dados";
const char* TOKEN_API = "seu_token_secreto";
const char* ENDPOINT_API_STATUS = "http://15.229.0.216:8080/api/status";
const char* ENDPOINT_API_COMANDOS_BASE = "http://15.229.0.216:8080/api/comandos";


void iniciarSpiffs() {
  if (!SPIFFS.begin()) {
    Serial.println("Falha ao montar o sistema de arquivos");
  }
}

void carregarCredenciais(String& ssid, String& password) {
  if (SPIFFS.exists("/config.txt")) {
    File configFile = SPIFFS.open("/config.txt", "r");
    if (configFile) {
      ssid = configFile.readStringUntil('\n');
      password = configFile.readStringUntil('\n');
      ssid.trim();
      password.trim();
      configFile.close();
      Serial.println("Credenciais carregadas.");
    } else {
      Serial.println("Erro ao abrir arquivo de configuração para leitura.");
    }
  } else {
    Serial.println("Arquivo de configuração não encontrado.");
  }
}

void salvarCredenciais(const String& ssid, const String& password) {
  File configFile = SPIFFS.open("/config.txt", "w");
  if (configFile) {
    configFile.println(ssid);
    configFile.println(password);
    configFile.close();
    Serial.println("Credenciais salvas.");
  } else {
    Serial.println("Erro ao abrir arquivo de configuração para escrita.");
  }
}

String obterDeviceId() {
  return WiFi.macAddress();
}

String obterEndpointApi() {
  return ENDPOINT_API;
}

String obterTokenApi() {
  return TOKEN_API;
}

String obterEndpointApiStatus() {
  return ENDPOINT_API_STATUS;
}

String obterEndpointApiComandosBase() {
  return ENDPOINT_API_COMANDOS_BASE;
}
