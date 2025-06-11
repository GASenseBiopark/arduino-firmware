#include "conexao_wifi.h"
#include "configuracao.h" // Para salvarCredenciais e obterCredenciais (implícito)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

ESP8266WebServer server(80);

// Intervalo para tentativas de reconexão
const unsigned long INTERVALO_RECONEXAO_WIFI = 30000; // 30 segundos
unsigned long ultimoTempoTentativaReconnect = 0;
unsigned int contadorReconexoesWifi = 0;

// Tenta conectar ao WiFi (usado no setup e na reconexão)
// Retorna true se conectado, false caso contrário.
// O parâmetro 'inicial' controla se é a tentativa inicial (com mais logs e timeout maior)
// ou uma tentativa de reconexão (mais silenciosa e rápida).
bool tentarConectarWiFi(const String& ssid, const String& password, bool inicial) {
  if (ssid.length() == 0) {
    if (inicial) Serial.println("SSID não configurado.");
    return false;
  }
  Serial.print(inicial ? "Tentando conectar ao WiFi SSID: " : "Tentando reconectar ao WiFi SSID: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  int tentativas = 0;
  // Timeout mais curto para reconexão para não bloquear o loop principal
  // Timeout mais longo para a conexão inicial
  int maxTentativas = inicial ? 20 : 10; // 20*500ms = 10s para inicial, 10*200ms = 2s para reconexão

  while (WiFi.status() != WL_CONNECTED && tentativas < maxTentativas) {
    delay(inicial ? 500 : 200);
    if (inicial) Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (inicial) {
      Serial.println("\nConectado ao WiFi!");
      Serial.print("Endereço IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("Reconectado ao WiFi!");
      Serial.print("Novo Endereço IP: ");
      Serial.println(WiFi.localIP());
      if (!inicial) { // Só incrementa se for uma reconexão (não a tentativa inicial)
        contadorReconexoesWifi++;
        Serial.print("Contador de reconexoes WiFi: ");
        Serial.println(contadorReconexoesWifi);
      }
    }
    return true;
  } else {
    if (inicial) Serial.println("\nFalha ao conectar ao WiFi na tentativa inicial.");
    // Não loga falha de reconexão toda vez para não poluir o serial.
    // Apenas a tentativa de reconexão será logada.
    WiFi.disconnect(true); // Garante que estamos desconectados para a próxima tentativa
    return false;
  }
}


// Função chamada no setup
bool conectarWiFi(const String& ssid, const String& password) {
  return tentarConectarWiFi(ssid, password, true);
}


void paginaConfig() {
  String html = "<html><head><title>Configuracao Wi-Fi</title></head><body>"
                "<h2>Configurar Wi-Fi</h2>"
                "<form method='POST' action='/salvar'>"
                "SSID: <input type='text' name='ssid'><br>"
                "Senha: <input type='password' name='password'><br>"
                "<input type='submit' value='Salvar'>"
                "</form></body></html>";
  server.send(200, "text/html", html);
}

void salvarWiFi() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  Serial.println("SSID: " + ssid);
  Serial.println("Password: " + password); // Cuidado ao logar senhas
  salvarCredenciais(ssid, password);
  server.send(200, "text/plain", "Configuracoes salvas. Reiniciando...");
  delay(1000);
  ESP.restart();
}

void iniciarAP() {
  Serial.println("Iniciando Modo AP (Access Point)...");
  WiFi.disconnect(true); // Garante que o modo STA seja desativado
  WiFi.mode(WIFI_AP);
  if (WiFi.softAP("ESP8266_Config", "senha123")) {
    Serial.println("Modo AP iniciado com sucesso.");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", HTTP_GET, paginaConfig); // Especificar HTTP_GET
    server.on("/salvar", HTTP_POST, salvarWiFi);
    server.begin();
    Serial.println("Servidor HTTP iniciado no modo AP.");
  } else {
    Serial.println("Falha ao iniciar o modo AP.");
    // Considere uma reinicialização ou outra forma de recuperação aqui
  }
}

void tratarClienteServidorWeb() {
  if (WiFi.getMode() == WIFI_AP) { // Só trata o cliente se estiver em modo AP
    server.handleClient();
  }
}

bool estaConectado() {
  return WiFi.status() == WL_CONNECTED;
}

void gerenciarConexaoWiFi(const String& ssid, const String& password) {
  if (!estaConectado() && WiFi.getMode() != WIFI_AP) { // Só tenta reconectar se não estiver em modo AP
    if (millis() - ultimoTempoTentativaReconnect >= INTERVALO_RECONEXAO_WIFI) {
      Serial.println("Conexao WiFi perdida. Tentando reconectar...");
      // Usa a função tentarConectarWiFi com 'inicial' = false para reconexão
      if (tentarConectarWiFi(ssid, password, false)) {
        // Se reconectar com sucesso, pode ser necessário reinicializar o NTP
        // ou outras tarefas que dependem da conexão.
        // Para simplificar, vamos assumir que o NTP será tratado no loop principal
        // se a conexão for restabelecida.
        // O incremento do contador já acontece dentro de tentarConectarWiFi se for uma reconexão bem sucedida.
      }
      ultimoTempoTentativaReconnect = millis();
    }
  }
}

unsigned int conexao_wifi_get_contador_reconexoes() {
  return contadorReconexoesWifi;
}
