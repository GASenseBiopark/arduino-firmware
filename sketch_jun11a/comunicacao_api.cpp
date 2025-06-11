#include "comunicacao_api.h"
#include "sensores.h" // Incluído para a função auxiliar de montagem de JSON
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <time.h>

// Constantes de comunicação
const long INTERVALO_ENVIO_API = 30000; // Para dados de sensores
unsigned long tempoUltimoEnvioAPI = 0;

const unsigned long INTERVALO_ENVIO_STATUS = 300000;
unsigned long ultimoEnvioStatus = 0;

const unsigned long INTERVALO_POLLING_COMANDOS = 60000; // 1 minuto em ms
unsigned long ultimoPollingComandos = 0;


// Configuração NTP
const char* servidorNTP = "pool.ntp.org";
const long  gmtOffset_sec = -3 * 3600; // GMT -3 (Brasilia)
const int   daylightOffset_sec = 0;    // Sem horário de verão

void inicializarNTP() {
  configTime(gmtOffset_sec, daylightOffset_sec, servidorNTP);
  Serial.println("Sincronizando hora com NTP...");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nHora sincronizada.");
}

String obterTimestampFormatado() {
  time_t now;
  time(&now);
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo); // Use gmtime_r para ser thread-safe, embora não estritamente necessário aqui

  char buffer[30];
  // Formato ISO8601: YYYY-MM-DDTHH:MM:SSZ
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buffer);
}

// Função auxiliar para montar o JSON - pode ser chamada pelo .ino
// Se o .ino montar o JSON diretamente, esta função pode não ser necessária externamente.
// No entanto, é bom tê-la para referência ou uso interno futuro.
String montarPayloadJson(const DadosSensores& dados, const String& deviceId) {
  StaticJsonDocument<256> doc; // Ajuste o tamanho conforme necessário
  doc["device_id"] = deviceId;
  doc["timestamp"] = obterTimestampFormatado();

  JsonObject data = doc.createNestedObject("data");
  data["temperatura"] = dados.temperatura;
  data["umidade"] = dados.umidade;
  data["nivel_gas"] = dados.nivelGas;
  data["chama_detectada"] = dados.presencaChama;

  String jsonOutput;
  serializeJson(doc, jsonOutput);
  return jsonOutput;
}


// Modificada para aceitar um payload JSON já formatado e retornar status
bool enviarDadosApi(const String& jsonPayload, const String& endpointApi, const String& tokenApi) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi nao conectado. Nao foi possivel enviar dados para a API.");
    return false; // Retorna false se não há conexão
  }

  // O controle de tempo (INTERVALO_ENVIO_API) foi mantido aqui, mas pode ser
  // desconsiderado ou gerenciado de forma diferente pelo chamador, especialmente para o buffer.
  // Se esta função for chamada para enviar dados do buffer, o chamador pode querer
  // ignorar este timer. Por enquanto, ele afeta envios diretos e de buffer igualmente.
  // Se o retorno do if abaixo for descomentado, ele impedirá envios frequentes do buffer.
  // Ele deve ser gerenciado pelo chamador (ex: no loop principal do .ino OU
  // ao decidir enviar dados do buffer). Se cada chamada a esta função deve
  // respeitar um intervalo global, o `tempoUltimoEnvioAPI` e a verificação
  // podem ser mantidos aqui. Para a funcionalidade de buffer, é melhor que
  // o chamador controle quando tentar enviar.
  // Para esta subtask, vamos manter o controle de tempo aqui para envios diretos,
  // mas o envio do buffer no .ino poderá ignorá-lo ou ter sua própria lógica.
  if (millis() - tempoUltimoEnvioAPI < INTERVALO_ENVIO_API && tempoUltimoEnvioAPI != 0) {
     //Serial.println("Intervalo de envio API ainda nao passou."); // Log opcional
     //return;
     // Decidi comentar o return para que o envio do buffer não seja impedido por este timer global.
     // O .ino controlará o envio do buffer. Este timer agora é mais para envios diretos.
  }
  // tempoUltimoEnvioAPI = millis(); // Movido para após o envio bem sucedido ou tentativa.


  WiFiClient client;
  HTTPClient http;
  bool sucessoEnvio = false;

  Serial.print("[HTTP] Iniciando requisicao para: ");
  Serial.println(endpointApi);

  if (http.begin(client, endpointApi)) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + tokenApi);

    Serial.print("[HTTP] Enviando JSON: ");
    Serial.println(jsonPayload);

    int httpCode = http.POST(jsonPayload);
    Serial.printf("[HTTP] Codigo de resposta: %d\n", httpCode);

    if (httpCode > 0) {
      tempoUltimoEnvioAPI = millis(); // Atualiza o tempo do último envio apenas se houve tentativa real
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        String responsePayload = http.getString();
        Serial.println("[HTTP] Resposta: " + responsePayload);
        sucessoEnvio = true;
      } else {
        // Códigos de erro HTTP (4xx, 5xx)
        Serial.printf("[HTTP] Falha no envio com codigo %d: %s\n", httpCode, http.errorToString(httpCode).c_str());
        // Não é necessário ler http.getString() para códigos de erro, a menos que a API retorne JSON de erro.
      }
    } else {
      // Erro na conexão HTTP antes de obter um código (ex: falha de DNS, conexão recusada)
      Serial.printf("[HTTP] Falha na requisicao (antes do codigo HTTP), erro: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.printf("[HTTP] Nao foi possivel conectar (http.begin falhou) para %s\n", endpointApi.c_str());
  }

  return sucessoEnvio;
}

String buscarComandosRemotos(const String& deviceId, const String& endpointCmdBase, const String& tokenApi) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[CMD_POLL] WiFi nao conectado. Nao foi possivel buscar comandos.");
    return ""; // Retorna string vazia indicando falha ou nenhum comando
  }

  WiFiClient client;
  HTTPClient http;
  String payload = ""; // Alterado para "" em vez de "[]" para erro/sem comando, "[]" para array vazio.

  String url = endpointCmdBase + "/" + deviceId;
  Serial.print("[CMD_POLL] Buscando comandos de: ");
  Serial.println(url);

  if (http.begin(client, url)) {
    http.addHeader("Authorization", "Bearer " + tokenApi);
    int httpCode = http.GET();
    Serial.printf("[CMD_POLL] Codigo de resposta: %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK) {
      payload = http.getString();
      if (payload.length() == 0 || payload == "null") {
        // Se a API retornar literalmente "null" ou string vazia para "sem comandos"
        payload = "[]"; // Normaliza para um array JSON vazio
      }
    } else if (httpCode == HTTP_CODE_NO_CONTENT) { // 204 No Content
      payload = "[]"; // Nenhum comando pendente
      Serial.println("[CMD_POLL] Nenhum comando novo (204 No Content).");
    } else {
      Serial.printf("[CMD_POLL] Falha ao buscar comandos, erro: %s\n", http.errorToString(httpCode).c_str());
      // payload permanece "" indicando erro
    }
    http.end();
  } else {
    Serial.printf("[CMD_POLL] Nao foi possivel conectar (http.begin falhou) para %s\n", url.c_str());
    // payload permanece "" indicando erro
  }

  return payload; // Pode ser "", "[]", ou "[{...}]"
}

String montarPayloadStatus(const String& deviceId, const String& ipAddress, int rssi, unsigned long uptimeSeconds, uint32_t freeHeap, int bufferedMessages, unsigned int wifiReconnects) {
  StaticJsonDocument<512> doc; // Aumentar tamanho se necessário

  doc["device_id"] = deviceId;
  doc["ip_address"] = ipAddress;
  doc["rssi"] = rssi;

  // Formatar uptime HH:MM:SS
  unsigned long secs = uptimeSeconds;
  unsigned long hours = secs / 3600;
  secs %= 3600;
  unsigned long mins = secs / 60;
  secs %= 60;
  char uptimeFormatted[12]; // Suficiente para "HHH:MM:SS\0"
  sprintf(uptimeFormatted, "%02lu:%02lu:%02lu", hours, mins, secs);
  doc["uptime"] = String(uptimeFormatted); // Ou apenas uptimeSeconds se preferir

  doc["free_heap"] = freeHeap;
  doc["buffered_messages"] = bufferedMessages;
  doc["wifi_reconnects"] = wifiReconnects;
  doc["timestamp"] = obterTimestampFormatado(); // Adiciona timestamp ao status também

  String jsonOutput;
  serializeJson(doc, jsonOutput);
  return jsonOutput;
}

bool enviarDadosStatus(const String& payloadStatusJson, const String& endpointApiStatus, const String& tokenApi) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi nao conectado. Nao foi possivel enviar dados de status.");
    return false;
  }

  // O controle de tempo para envio de status será feito no .ino
  // usando a variável global `ultimoEnvioStatus` e `INTERVALO_ENVIO_STATUS`

  WiFiClient client;
  HTTPClient http;
  bool sucessoEnvio = false;

  Serial.print("[HTTP_STATUS] Iniciando requisicao para: ");
  Serial.println(endpointApiStatus);

  if (http.begin(client, endpointApiStatus)) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + tokenApi); // Usando o mesmo token

    Serial.print("[HTTP_STATUS] Enviando JSON: ");
    Serial.println(payloadStatusJson);

    int httpCode = http.POST(payloadStatusJson);
    Serial.printf("[HTTP_STATUS] Codigo de resposta: %d\n", httpCode);

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        String responsePayload = http.getString();
        Serial.println("[HTTP_STATUS] Resposta: " + responsePayload);
        sucessoEnvio = true;
      } else {
        Serial.printf("[HTTP_STATUS] Falha no envio com codigo %d: %s\n", httpCode, http.errorToString(httpCode).c_str());
      }
    } else {
      Serial.printf("[HTTP_STATUS] Falha na requisicao (antes do codigo HTTP), erro: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.printf("[HTTP_STATUS] Nao foi possivel conectar (http.begin falhou) para %s\n", endpointApiStatus.c_str());
  }

  return sucessoEnvio;
}
