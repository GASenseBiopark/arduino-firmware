#include <WiFi.h>
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

// Conexão da rede Wifi - 2.4 MHz
const char* ssid = "BPK-ALUNOS";
const char* password = "2020alunos";

// Endpoint da API
const char* apiEndpoint = "http://15.229.0.216:8080/gravarLeituras";
const char* token = "mX9$wP7#qR2!vB8@zLtF4&GjKdY1NcU"; 

// Intervalos
const unsigned long INTERVALO_NORMAL = 15000; // 15s
const unsigned long INTERVALO_ALERTA = 1000;  // 1s

unsigned long ultimoEnvio = 0;

// Limites aceitáveis
const int LIMITE_MQ2 = 600;
const int LIMITE_MQ4 = 600;
const int LIMITE_MQ135 = 600;
const bool LIMITE_CHAMA = true; // true = fogo detectado

// Configuração do servidor NTP
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800;    // UTC-3 (horário de Brasília)
const int daylightOffset_sec = 0;  // Sem horário de verão

void setup() {
  Serial.begin(115200);

  pinMode(MQ2_PIN, INPUT);
  pinMode(MQ4_PIN, INPUT);
  pinMode(MQ135_PIN, INPUT);
  pinMode(KY026_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  dht.begin();

  WiFi.begin(ssid, password);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");

  // Inicializa o NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
  unsigned long agora = millis();

  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();

  int gas_mq2 = analogRead(MQ2_PIN);
  int gas_mq4 = analogRead(MQ4_PIN);
  int gas_mq135 = analogRead(MQ135_PIN);
  bool chama = (digitalRead(KY026_PIN) == LOW); // LOW indica chama detectada

  // Detectar risco
  bool risco = (gas_mq2 > LIMITE_MQ2 || gas_mq4 > LIMITE_MQ4 || gas_mq135 > LIMITE_MQ135 || chama == LIMITE_CHAMA);

  // Buzzer
  digitalWrite(BUZZER_PIN, risco ? HIGH : LOW);

  // Envio de dados:
  unsigned long intervalo = risco ? INTERVALO_ALERTA : INTERVALO_NORMAL;

  if (agora - ultimoEnvio >= intervalo) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(apiEndpoint);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("Authorization", String(token));

      // Obter a hora atual formatada em ISO 8601
      struct tm timeinfo;
      if (!getLocalTime(&timeinfo)) {
        Serial.println("Erro ao obter hora");
        return;
      }

      char dataHoraISO[30];
      strftime(dataHoraISO, sizeof(dataHoraISO), "%Y-%m-%dT%H:%M:%S", &timeinfo);

      StaticJsonDocument<256> body;
      body["data_hora"] = dataHoraISO;
      body["temperatura"] = 32;
      body["umidade"] = 55;
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
