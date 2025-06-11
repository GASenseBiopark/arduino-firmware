#ifndef CONEXAO_WIFI_H
#define CONEXAO_WIFI_H

#include <Arduino.h>

void iniciarAP();
bool conectarWiFi(const String& ssid, const String& password); // Modificado para retornar bool
void tratarClienteServidorWeb();
// bool verificarStatusWifi(); // Substituído por estaConectado()
bool estaConectado(); // Novo nome para maior clareza
void paginaConfig();
void salvarWiFi();
void gerenciarConexaoWiFi(const String& ssid, const String& password);
unsigned int conexao_wifi_get_contador_reconexoes(); // Nova função

// Variável para timestamp de reconexão
extern unsigned long ultimoTempoTentativaReconnect;
// Variável para contador de reconexões
extern unsigned int contadorReconexoesWifi;

#endif
