#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "time.h"

#define MQ2_PIN 35
#define MQ4_PIN 32
#define MQ135_PIN 34
#define DHT11_PIN 4
#define KY026_PIN 14
#define BUZZER_PIN 27

#define DHTTYPE DHT11
DHT dht(DHT11_PIN, DHTTYPE);

// Variáveis globais
WebServer server(80);
bool wifiConectado = false;

String ssid = "";
String password = "";

// Endpoint da API e token fixos
const char* apiEndpoint = "http://15.229.0.216:8080/gravarLeituras";
const char* token = "mX9$wP7#qR2!vB8@zLtF4&GjKdY1NcU";

// Intervalos
const unsigned long INTERVALO_NORMAL = 15000; // 15s
const unsigned long INTERVALO_ALERTA = 1000;  // 1s
unsigned long ultimoEnvio = 0;

// Limites
const int LIMITE_MQ2 = 600;
const int LIMITE_MQ4 = 600;
const int LIMITE_MQ135 = 600;
const bool LIMITE_CHAMA = true; // true = fogo detectado

// Configuração do servidor NTP
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800;    // UTC-3 (horário de Brasília)
const int daylightOffset_sec = 0;      // Sem horário de verão

// Função para carregar WiFi salvo do SPIFFS
bool carregarCredenciais() {
  if(!SPIFFS.begin(true)){
    Serial.println("Erro ao montar SPIFFS");
    return false;
  }
  if(!SPIFFS.exists("/wifi.json")) {
    Serial.println("Arquivo de WiFi não existe");
    return false;
  }
  File file = SPIFFS.open("/wifi.json", "r");
  if(!file) {
    Serial.println("Erro ao abrir arquivo de WiFi");
    return false;
  }
  size_t size = file.size();
  std::unique_ptr<char[]> buf(new char[size]);
  file.readBytes(buf.get(), size);
  file.close();

  StaticJsonDocument<256> doc;
  auto err = deserializeJson(doc, buf.get());
  if(err) {
    Serial.println("Erro ao desserializar JSON");
    return false;
  }
  ssid = String((const char*)doc["ssid"]);
  password = String((const char*)doc["password"]);
  Serial.printf("Credenciais carregadas: SSID=%s\n", ssid.c_str());
  return true;
}

// Salvar credenciais na SPIFFS
void salvarCredenciais(String newSsid, String newPass) {
  StaticJsonDocument<256> doc;
  doc["ssid"] = newSsid;
  doc["password"] = newPass;

  File file = SPIFFS.open("/wifi.json", "w");
  if(!file) {
    Serial.println("Erro ao abrir arquivo para salvar WiFi");
    return;
  }
  serializeJson(doc, file);
  file.close();
  Serial.println("Credenciais WiFi salvas");
  ssid = newSsid;
  password = newPass;
}

// Página para configuração WiFi
void paginaConfig() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Configurar WiFi</title></head><body>";
  html += "<h1>Configurar WiFi</h1>";
  html += "<form action='/salvar' method='POST'>";
  html += "SSID: <input type='text' name='ssid' required><br>";
  html += "Senha: <input type='password' name='password' required><br>";
  html += "<input type='submit' value='Salvar'>";
  html += "</form></body></html>";
  server.send(200, "text/html", html);
}

// Rota para salvar credenciais
void salvarWiFi() {
  if(server.hasArg("ssid") && server.hasArg("password")) {
    String newSsid = server.arg("ssid");
    String newPass = server.arg("password");

    salvarCredenciais(newSsid, newPass);

    String resposta = "<!DOCTYPE html><html><body><h1>Credenciais salvas!</h1><p>Reinicie o dispositivo para conectar.</p></body></html>";
    server.send(200, "text/html", resposta);
  } else {
    server.send(400, "text/plain", "Dados inválidos");
  }
}

// Tenta conectar WiFi com SSID e senha salvos
bool conectarWiFi() {
  if(ssid.length() == 0) {
    Serial.println("SSID vazio, não tenta conectar");
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.printf("Tentando conectar a %s\n", ssid.c_str());

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("Falha ao conectar WiFi");
    return false;
  }
}

void iniciarAP() {
  WiFi.mode(WIFI_AP);
  const char* apSSID = "Configurar Dispositivo GASense";
  bool apOk = WiFi.softAP(apSSID);
  if(apOk) {
    Serial.printf("AP iniciado: %s\n", apSSID);
  } else {
    Serial.println("Erro ao iniciar AP");
  }
  IPAddress IP = WiFi.softAPIP();
  Serial.print("IP do AP: ");
  Serial.println(IP);

  server.on("/", paginaConfig);
  server.on("/salvar", HTTP_POST, salvarWiFi);
  server.begin();
  Serial.println("Servidor web iniciado");
}

void setup() {
  Serial.begin(115200);
  pinMode(MQ2_PIN, INPUT);
  pinMode(MQ4_PIN, INPUT);
  pinMode(MQ135_PIN, INPUT);
  pinMode(KY026_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  dht.begin();

  // Tenta carregar credenciais
  if(!carregarCredenciais() || !conectarWiFi()) {
    // Se falhar, inicia AP para configuração
    iniciarAP();
  } else {
    wifiConectado = true;
    // Inicializa NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  }
}

void loop() {
  if (!wifiConectado) {
    server.handleClient();
    return;
  }

  unsigned long agora = millis();

  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();

  int gas_mq2 = analogRead(MQ2_PIN);
  int gas_mq4 = analogRead(MQ4_PIN);
  int gas_mq135 = analogRead(MQ135_PIN);
  bool chama = (digitalRead(KY026_PIN) == LOW);

  bool risco = (gas_mq2 > LIMITE_MQ2 || gas_mq4 > LIMITE_MQ4 || gas_mq135 > LIMITE_MQ135 || chama == LIMITE_CHAMA);

  digitalWrite(BUZZER_PIN, risco ? HIGH : LOW);

  unsigned long intervalo = risco ? INTERVALO_ALERTA : INTERVALO_NORMAL;

  if (agora - ultimoEnvio >= intervalo) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(apiEndpoint);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("Authorization", String(token));

      struct tm timeinfo;
      if (!getLocalTime(&timeinfo)) {
        Serial.println("Erro ao obter hora");
        return;
      }

      char dataHoraISO[30];
      strftime(dataHoraISO, sizeof(dataHoraISO), "%Y-%m-%dT%H:%M:%S", &timeinfo);

      StaticJsonDocument<256> body;
      body["data_hora"] = dataHoraISO;
      body["temperatura"] = temperatura;
      body["umidade"] = umidade;
      body["fogo"] = chama;
      body["gas_glp"] = gas_mq2;
      body["compostos_toxicos"] = gas_mq135;
      body["gas_metano"] = gas_mq4;

      String json;
      serializeJson(body, json);

      int response = http.POST(json);

      Serial.println("Payload enviado:");
      Serial.println(json);
      Serial.print("Código de resposta: ");
      Serial.println(response);

      http.end();
    }
    ultimoEnvio = agora;
  }
}
