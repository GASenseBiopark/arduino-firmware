#ifndef BUFFER_DADOS_H
#define BUFFER_DADOS_H

#include <Arduino.h>
#include "sensores.h" // Para DadosSensores

// Nome do arquivo de buffer e limite de registros
extern const char* NOME_ARQUIVO_BUFFER;
extern const int LIMITE_REGISTROS_BUFFER;

// Funções para buffer
void buffer_dados_inicializar();
bool buffer_dados_salvar_leitura(const String& leituraJson);
String buffer_dados_ler_proxima_leitura();
bool buffer_dados_remover_leitura_enviada();
bool buffer_dados_tem_dados();
int buffer_dados_contar_registros();

// Funções anteriores que podem ser adaptadas ou removidas se não usadas diretamente pelo .ino
void adicionarAoBuffer(const DadosSensores& dados); // Pode ser interna ou modificada
void enviarBufferParaApi(); // Esta lógica será movida para o .ino principal

#endif
