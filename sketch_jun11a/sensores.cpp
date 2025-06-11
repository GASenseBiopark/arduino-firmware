#include "sensores.h"
#include <DHT.h>

// Definições de pinos dos sensores
#define DHTPIN D4        // Pino do sensor DHT22
#define MQ2PIN A0        // Pino do sensor MQ-2 (analógico)
#define FLAMEPIN D5    // Pino do sensor de chama (digital)
#define BUZZERPIN D6   // Pino do buzzer

// Constantes de limite dos sensores
const int LIMITE_GAS = 400;       // Limite para o sensor de gás MQ-2
const float LIMITE_TEMP = 30.0;   // Limite de temperatura em Celsius
const float LIMITE_UMIDADE = 70.0; // Limite de umidade em %

// Objeto DHT
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Variáveis e constantes para silenciar buzzer
bool buzzerSilenciado = false;
unsigned long tempoInicioSilencioBuzzer = 0;
const unsigned long DURACAO_SILENCIO_BUZZER_MS_PADRAO = 300000; // 5 minutos


void inicializarSensores() {
  pinMode(MQ2PIN, INPUT);
  pinMode(FLAMEPIN, INPUT);
  pinMode(BUZZERPIN, OUTPUT);
  digitalWrite(BUZZERPIN, LOW); // Buzzer desligado inicialmente
  dht.begin();
  Serial.println("Sensores inicializados.");
}

DadosSensores lerDadosSensores() {
  DadosSensores dados;
  dados.temperatura = dht.readTemperature();
  dados.umidade = dht.readHumidity();
  dados.nivelGas = analogRead(MQ2PIN);
  dados.presencaChama = digitalRead(FLAMEPIN) == LOW; // Sensor de chama digital: LOW indica detecção

  // Verifica se as leituras do DHT são válidas
  if (isnan(dados.temperatura) || isnan(dados.umidade)) {
    Serial.println("Falha ao ler do sensor DHT!");
    // Pode-se definir valores padrão ou manter os NaN para tratamento posterior
    dados.temperatura = -999.0; // Valor inválido
    dados.umidade = -999.0;    // Valor inválido
  }

  Serial.print("Temperatura: ");
  Serial.print(dados.temperatura);
  Serial.print(" *C, Umidade: ");
  Serial.print(dados.umidade);
  Serial.print(" %, Gas: ");
  Serial.print(dados.nivelGas);
  Serial.print(", Chama: ");
  Serial.println(dados.presencaChama ? "Detectada" : "Nao detectada");

  return dados;
}

bool verificarCondicaoRisco(const DadosSensores& dados) {
  bool risco = false;
  if (dados.temperatura > LIMITE_TEMP) {
    Serial.println("RISCO: Temperatura ALTA!");
    risco = true;
  }
  if (dados.umidade > LIMITE_UMIDADE) {
    Serial.println("RISCO: Umidade ALTA!");
    risco = true;
  }
  if (dados.nivelGas > LIMITE_GAS) {
    Serial.println("RISCO: Nivel de GAS ALTO!");
    risco = true;
  }
  if (dados.presencaChama) {
    Serial.println("RISCO: CHAMA DETECTADA!");
    risco = true;
  }
  return risco;
}

void sensores_silenciar_buzzer_temporariamente(unsigned long duracaoMs) {
  buzzerSilenciado = true;
  tempoInicioSilencioBuzzer = millis();
  digitalWrite(BUZZERPIN, LOW); // Garante que o buzzer seja desligado ao silenciar
  Serial.print("Buzzer silenciado por ");
  Serial.print(duracaoMs / 1000);
  Serial.println(" segundos.");
  // Nota: A DURACAO_SILENCIO_BUZZER_MS_PADRAO é usada em verificar_buzzer_silenciado,
  // mas a função aceita uma duração que pode ser usada para um temporizador mais dinâmico se necessário.
  // Para o comando simples, usaremos a duração padrão definida em sensores_verificar_buzzer_silenciado.
  // Se for necessário usar duracaoMs aqui, a lógica em verificar_buzzer_silenciado precisaria mudar.
  // Por simplicidade, o comando remoto usará a duração padrão.
}

void sensores_reativar_buzzer() {
  buzzerSilenciado = false;
  Serial.println("Buzzer reativado (pode soar na proxima condicao de risco).");
}

void sensores_verificar_buzzer_silenciado() {
  if (buzzerSilenciado && (millis() - tempoInicioSilencioBuzzer >= DURACAO_SILENCIO_BUZZER_MS_PADRAO)) {
    Serial.println("Tempo de silencio do buzzer expirou. Buzzer reativado.");
    buzzerSilenciado = false;
  }
}

void controlarBuzzer(bool ligar) {
  sensores_verificar_buzzer_silenciado(); // Verifica se o silêncio expirou

  if (ligar && !buzzerSilenciado) {
    digitalWrite(BUZZERPIN, HIGH);
  } else {
    digitalWrite(BUZZERPIN, LOW);
  }
}
