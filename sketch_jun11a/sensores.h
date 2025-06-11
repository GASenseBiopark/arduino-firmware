#ifndef SENSORES_H
#define SENSORES_H

#include <Arduino.h>

struct DadosSensores {
  float temperatura;
  float umidade;
  int nivelGas;
  bool presencaChama;
};

void inicializarSensores();
DadosSensores lerDadosSensores();
bool verificarCondicaoRisco(const DadosSensores& dados);
void controlarBuzzer(bool ligar); // Esta será modificada para considerar o silêncio

// Para silenciar buzzer
extern bool buzzerSilenciado;
extern unsigned long tempoInicioSilencioBuzzer;
extern const unsigned long DURACAO_SILENCIO_BUZZER_MS_PADRAO; // Adicionado _PADRAO

void sensores_silenciar_buzzer_temporariamente(unsigned long duracaoMs);
void sensores_verificar_buzzer_silenciado();
void sensores_reativar_buzzer();

#endif
