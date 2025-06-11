#include "buffer_dados.h"
#include "comunicacao_api.h" // Para enviarDadosApi (se usado internamente)
#include "configuracao.h"   // Para obterEndpointApi, obterTokenApi, obterDeviceId (se usado internamente)
#include <FS.h>             // Para SPIFFS

// Definição do nome do arquivo de buffer e limite
const char* NOME_ARQUIVO_BUFFER = "/buffer_leituras.txt";
const int LIMITE_REGISTROS_BUFFER = 100; // Exemplo, pode ser ajustado

// Implementação das funções de buffer

void buffer_dados_inicializar() {
  Serial.println("Buffer de dados SPIFFS inicializado. Verificando SPIFFS...");
  // SPIFFS.begin() já deve ter sido chamado em configuracao.cpp -> iniciarSpiffs()
  // Apenas uma mensagem de log aqui.
  if (SPIFFS.exists(NOME_ARQUIVO_BUFFER)) {
    Serial.print("Arquivo de buffer existente encontrado. Registros atuais: ");
    Serial.println(buffer_dados_contar_registros());
  } else {
    Serial.println("Arquivo de buffer nao encontrado. Sera criado se necessario.");
  }
}

int buffer_dados_contar_registros() {
  if (!SPIFFS.exists(NOME_ARQUIVO_BUFFER)) {
    return 0;
  }
  File file = SPIFFS.open(NOME_ARQUIVO_BUFFER, "r");
  if (!file) {
    Serial.println("Erro ao abrir arquivo de buffer para contagem.");
    return 0;
  }
  int count = 0;
  while (file.available()) {
    file.readStringUntil('\n'); // Lê a linha
    count++;
  }
  file.close();
  return count;
}

bool buffer_dados_salvar_leitura(const String& leituraJson) {
  if (leituraJson.length() == 0) {
    Serial.println("Tentativa de salvar leitura vazia no buffer.");
    return false;
  }

  int current_records = buffer_dados_contar_registros();
  if (current_records >= LIMITE_REGISTROS_BUFFER) {
    Serial.print("Buffer cheio (");
    Serial.print(current_records);
    Serial.print("/");
    Serial.print(LIMITE_REGISTROS_BUFFER);
    Serial.println("). Nao foi possivel salvar a leitura.");
    // Estratégia de remoção do mais antigo poderia ser implementada aqui.
    // Por exemplo, chamar buffer_dados_remover_leitura_enviada() e depois tentar salvar novamente.
    return false;
  }

  File file = SPIFFS.open(NOME_ARQUIVO_BUFFER, "a"); // Modo append
  if (!file) {
    Serial.println("Erro ao abrir arquivo de buffer para escrita (append).");
    return false;
  }

  if (file.println(leituraJson)) {
    Serial.println("Leitura salva no buffer SPIFFS.");
    file.close();
    return true;
  } else {
    Serial.println("Erro ao escrever leitura no buffer SPIFFS.");
    file.close();
    return false;
  }
}

bool buffer_dados_tem_dados() {
  if (SPIFFS.exists(NOME_ARQUIVO_BUFFER)) {
    File file = SPIFFS.open(NOME_ARQUIVO_BUFFER, "r");
    if (file && file.size() > 0) {
      file.close();
      return true;
    }
    if (file) file.close();
  }
  return false;
}

String buffer_dados_ler_proxima_leitura() {
  if (!buffer_dados_tem_dados()) {
    return ""; // String vazia se não há dados
  }

  File file = SPIFFS.open(NOME_ARQUIVO_BUFFER, "r");
  if (!file) {
    Serial.println("Erro ao abrir arquivo de buffer para leitura.");
    return "";
  }

  String primeiraLinha = "";
  if (file.available()) {
    primeiraLinha = file.readStringUntil('\n');
    primeiraLinha.trim(); // Remove \r ou \n extras se houver
  }
  file.close();
  return primeiraLinha;
}

bool buffer_dados_remover_leitura_enviada() {
  if (!buffer_dados_tem_dados()) {
    Serial.println("Nenhuma leitura no buffer para remover.");
    return false; // Nada a fazer
  }

  File originalFile = SPIFFS.open(NOME_ARQUIVO_BUFFER, "r");
  if (!originalFile) {
    Serial.println("Erro ao abrir arquivo de buffer original para remocao.");
    return false;
  }

  const char* tempFileName = "/buffer_temp.txt";
  File tempFile = SPIFFS.open(tempFileName, "w");
  if (!tempFile) {
    Serial.println("Erro ao criar arquivo de buffer temporario.");
    originalFile.close();
    return false;
  }

  bool primeiraLinhaPulada = false;
  while (originalFile.available()) {
    String linha = originalFile.readStringUntil('\n');
    if (!primeiraLinhaPulada) {
      primeiraLinhaPulada = true;
      // Pula a escrita da primeira linha no arquivo temporário
      continue;
    }
    // Escreve as linhas subsequentes no arquivo temporário
    // Adiciona \n de volta pois readStringUntil o consome
    tempFile.print(linha + "\n");
  }
  originalFile.close();
  tempFile.close();

  // Deleta o arquivo original e renomeia o temporário
  if (!SPIFFS.remove(NOME_ARQUIVO_BUFFER)) {
    Serial.println("Erro ao remover arquivo de buffer original.");
    // Tenta remover o temporário para limpeza, mas o erro principal já ocorreu.
    SPIFFS.remove(tempFileName);
    return false;
  }

  if (!SPIFFS.rename(tempFileName, NOME_ARQUIVO_BUFFER)) {
    Serial.println("Erro ao renomear arquivo de buffer temporario.");
    // O arquivo original foi removido, mas o temporário não pôde ser renomeado.
    // Isso é um estado problemático. Tentar recriar um arquivo vazio pode ser uma opção.
    return false;
  }

  Serial.println("Primeira leitura removida do buffer com sucesso.");
  return true;
}


// --- Implementações anteriores que podem precisar de ajuste/remoção ---

// Esta função agora é mais um conceito, já que o .ino vai construir o JSON
// e passá-lo para buffer_dados_salvar_leitura.
// Se for para manter, ela precisaria construir o JSON aqui.
void adicionarAoBuffer(const DadosSensores& dados) {
  Serial.println("Funcao 'adicionarAoBuffer(DadosSensores)' chamada - OBSOLETA. Use 'buffer_dados_salvar_leitura(String json)'.");
  // Exemplo de como poderia ser se ela construísse o JSON:
  // String jsonPayload = ""; // ... construir JSON a partir de 'dados' ...
  // buffer_dados_salvar_leitura(jsonPayload);
}

// A lógica de enviar o buffer agora será gerenciada no .ino principal,
// lendo do buffer e usando comunicacao_api.enviarDadosApi.
void enviarBufferParaApi() {
  Serial.println("Funcao 'enviarBufferParaApi()' chamada - OBSOLETA. Logica movida para o loop principal.");
  // while(buffer_dados_tem_dados()) {
  //   String leitura = buffer_dados_ler_proxima_leitura();
  //   if (leitura.length() > 0) {
  //     // Aqui precisaria obter deviceId, endpoint, token
  //     // e chamar a função de envio da comunicacao_api
  //     // Se o envio for bem sucedido, chamar buffer_dados_remover_leitura_enviada();
  //   }
  // }
}
