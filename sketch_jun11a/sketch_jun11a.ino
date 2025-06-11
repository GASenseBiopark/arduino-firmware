#include "configuracao.h"
#include "conexao_wifi.h"
#include "sensores.h"
#include "comunicacao_api.h"
#include "buffer_dados.h"

// Variáveis globais para credenciais Wi-Fi
String ssid = "";
String password = "";

// Variáveis de configuração da API (serão obtidas de configuracao.cpp)
String deviceId = "";
String endpointApi = "";
String tokenApi = "";
String endpointApiStatus = "";
String endpointCmdBase = "";   // Para o endpoint de comandos

// Variáveis para controle de tempo de envio de status
unsigned long ultimoEnvioStatus = 0;
const unsigned long INTERVALO_ENVIO_STATUS = 300000; // 5 minutos

// Variáveis para controle de tempo de polling de comandos
unsigned long ultimoPollingComandos = 0;
const unsigned long INTERVALO_POLLING_COMANDOS = 60000; // 1 minuto


void setup() {
  Serial.begin(115200);
  Serial.println("\nIniciando dispositivo...");

  Serial.println("Gerenciando Watchdog Timer (ESP8266)...");
  // O WDT de software é habilitado por padrão.
  // ESP.wdtEnable(timeout_ms); // Opcional: para configurar um timeout específico.
  // Por agora, vamos apenas garantir que ele seja alimentado no loop.

  Serial.println("Executando Setup...");
  iniciarSpiffs();
  carregarCredenciais(ssid, password); // Carrega ssid e password globalmente

  if (ssid.length() > 0 && password.length() > 0) {
    Serial.println("Credenciais encontradas. Tentando conectar ao WiFi...");
    if (conectarWiFi(ssid, password)) { // Tenta conectar com as credenciais carregadas
      inicializarNTP(); // Sincronizar hora se conectado ao WiFi
    } else {
      Serial.println("Falha ao conectar com credenciais salvas. Iniciando Modo AP.");
      iniciarAP(); // Inicia AP se a conexão inicial falhar
    }
  } else {
    Serial.println("Nenhuma credencial Wi-Fi salva. Iniciando Modo AP para configuracao.");
    iniciarAP();
  }

  inicializarSensores();
  // buffer_dados_inicializar() é chamado aqui, após SPIFFS e antes de qualquer uso do buffer.
  // A declaração original de inicializarBuffer() em buffer_dados.h/cpp foi renomeada para buffer_dados_inicializar()
  buffer_dados_inicializar();

  deviceId = obterDeviceId();
  endpointApi = obterEndpointApi();
  tokenApi = obterTokenApi();
  endpointApiStatus = obterEndpointApiStatus();
  endpointCmdBase = obterEndpointApiComandosBase(); // Obter o endpoint de comandos

  Serial.print("Device ID: ");
  Serial.println(deviceId);
  Serial.print("Endpoint API: ");
  Serial.println(endpointApi);
  // Não imprima o token da API em produção por segurança
  // Serial.print("Token API: ");
  // Serial.println(tokenApi);

  Serial.println("Setup concluido.");
}

void loop() {
  //Serial.println("Loop principal..."); // Log muito verboso para o loop

  if (WiFi.getMode() == WIFI_AP) {
    tratarClienteServidorWeb(); // Lida com requisições HTTP se estiver em modo AP
  } else {
    // Não está em modo AP, então gerencia a conexão Wi-Fi (tentativas de reconexão)
    // As credenciais ssid e password (globais neste arquivo) são usadas pela gerenciarConexaoWiFi
    gerenciarConexaoWiFi(ssid, password);
  }

  // Sempre lê os sensores, independente do status da conexão, para acionar o buzzer localmente
  DadosSensores dadosAtuais = lerDadosSensores();
  bool condicaoDeRisco = verificarCondicaoRisco(dadosAtuais);
  controlarBuzzer(condicaoDeRisco);

  // Montar o payload JSON usando a função de comunicacao_api
  String payloadJson = montarPayloadJson(dadosAtuais, deviceId);

  if (estaConectado()) {
    Serial.println("WiFi Conectado.");
    // 1. Tentar enviar dados do buffer primeiro
    if (buffer_dados_tem_dados()) {
      Serial.println("Enviando dados do buffer...");
      int contadorEnviosBuffer = 0;
      while (buffer_dados_tem_dados() && estaConectado() && contadorEnviosBuffer < 5) { // Limita envios por ciclo
        String leituraDoBuffer = buffer_dados_ler_proxima_leitura();
        if (leituraDoBuffer.length() > 0) {
          // A função enviarDadosApi agora aceita o JSON diretamente.
          // O controle de tempo interno de enviarDadosApi pode ser um problema aqui.
          // Idealmente, para envio de buffer, esse controle de tempo deveria ser bypassado
          // ou a função enviarDadosApi não deveria ter tal controle.
          // Por agora, vamos chamá-la. Se o envio do buffer for lento devido ao timer interno,
          // o timer em enviarDadosApi precisará ser removido ou condicional.
          Serial.print("Enviando do buffer: ");
          Serial.println(leituraDoBuffer);
          bool envioBufferOk = enviarDadosApi(leituraDoBuffer, endpointApi, tokenApi);

          if (envioBufferOk) {
            Serial.println("Leitura do buffer enviada com sucesso. Removendo do buffer.");
            buffer_dados_remover_leitura_enviada();
          } else {
            Serial.println("Falha ao enviar leitura do buffer. Mantera no buffer.");
            // Se o envio falhar, paramos de tentar enviar o buffer neste ciclo para evitar spamming
            // e para dar chance da conexão/API se recuperar.
            break;
          }
          contadorEnviosBuffer++;
        }
      }
      if(buffer_dados_tem_dados()){
         Serial.println("Ainda ha dados no buffer para enviar em proximos ciclos.");
      } else {
         if (contadorEnviosBuffer > 0) Serial.println("Buffer de dados esvaziado.");
      }
    }

    // 2. Enviar leitura atual
    Serial.println("Tentando enviar leitura atual...");
    if (enviarDadosApi(payloadJson, endpointApi, tokenApi)) {
      Serial.println("Leitura atual enviada com sucesso para API.");
    } else {
      Serial.println("Falha ao enviar leitura atual para API. Salvando no buffer...");
      if (buffer_dados_salvar_leitura(payloadJson)) {
        Serial.println("Leitura atual salva no buffer devido a falha no envio.");
      } else {
        Serial.println("Falha ao salvar leitura atual no buffer (pode estar cheio).");
      }
    }

    // Se NTP não sincronizou no setup (ex: WiFi caiu antes de NTP), tentar novamente
    // Uma forma simples é verificar se o tempo é válido (distante de 1970)
    // Esta lógica de ressincronização do NTP pode ser melhorada.
    if (time(nullptr) < 1000000000) { // Se o tempo parecer inválido (ex: perto do epoch)
        Serial.println("NTP não sincronizado ou perdido, tentando sincronizar novamente...");
        inicializarNTP();
    }

    // Envio de Status
    // Usa INTERVALO_ENVIO_STATUS definido neste arquivo (.ino)
    if (millis() - ultimoEnvioStatus >= INTERVALO_ENVIO_STATUS || ultimoEnvioStatus == 0) {
        Serial.println("Preparando para enviar dados de status...");
        String ip = WiFi.localIP().toString();
        long rssi = WiFi.RSSI();
        unsigned long uptime = millis() / 1000; // Uptime em segundos
        uint32_t heap = ESP.getFreeHeap();
        int bufferCount = buffer_dados_contar_registros();
        unsigned int reconnections = conexao_wifi_get_contador_reconexoes();

        String statusPayload = montarPayloadStatus(deviceId, ip, rssi, uptime, heap, bufferCount, reconnections);

        // Usar endpointApiStatus e tokenApi obtidos no setup
        if (enviarDadosStatus(statusPayload, endpointApiStatus, tokenApi)) {
            Serial.println("Dados de status enviados com sucesso.");
        } else {
            Serial.println("Falha ao enviar dados de status.");
        }
        ultimoEnvioStatus = millis();
    }

    // Polling de Comandos Remotos
    // Usa INTERVALO_POLLING_COMANDOS definido neste arquivo (.ino)
    // Usa ultimoPollingComandos definido neste arquivo (.ino)
    if (millis() - ultimoPollingComandos >= INTERVALO_POLLING_COMANDOS || ultimoPollingComandos == 0) {
        Serial.println("Verificando comandos remotos...");
        String comandos = buscarComandosRemotos(deviceId, endpointCmdBase, tokenApi);

        if (comandos.length() > 0) {
            Serial.print("Resposta da API de comandos: ");
            Serial.println(comandos);
            if (comandos != "[]") {
                 processarComandosRecebidos(comandos);
            } else {
                Serial.println("Nenhum comando novo (array vazio).");
            }
        } else {
            Serial.println("Nenhuma resposta ou erro ao buscar comandos.");
        }
        ultimoPollingComandos = millis();
    }

  } else {
    // Se não estiver conectado ao Wi-Fi (e não em modo AP):
    Serial.println("WiFi desconectado.");
    Serial.println("Salvando leitura atual no buffer SPIFFS...");
    if (buffer_dados_salvar_leitura(payloadJson)) {
      Serial.println("Leitura atual salva no buffer.");
    } else {
      Serial.println("Falha ao salvar leitura atual no buffer (pode estar cheio).");
    }
    // A antiga chamada adicionarAoBuffer(dadosAtuais) foi substituída pela de cima.
  }

  // Pequeno delay para estabilidade e para permitir que tarefas de baixo nível (como WiFi) rodem.
  delay(1000);
  ESP.wdtFeed(); // Alimenta o watchdog timer do ESP8266
}

void processarComandosRecebidos(const String& comandosJson) {
  if (comandosJson.length() == 0 || comandosJson == "[]") {
    // Serial.println("Nenhum comando para processar ou JSON vazio."); // Log opcional
    return;
  }

  StaticJsonDocument<1024> doc; // Ajustar tamanho conforme o tamanho máximo esperado do JSON de comandos
  DeserializationError error = deserializeJson(doc, comandosJson);

  if (error) {
    Serial.print("Falha ao parsear JSON de comandos: ");
    Serial.println(error.c_str());
    return;
  }

  if (!doc.is<JsonArray>()) {
    Serial.println("JSON de comandos nao e um array.");
    return;
  }

  JsonArray arrayDeComandos = doc.as<JsonArray>();
  if (arrayDeComandos.isNull() || arrayDeComandos.size() == 0) {
    Serial.println("Array de comandos vazio ou invalido.");
    return;
  }

  Serial.print("Processando ");
  Serial.print(arrayDeComandos.size());
  Serial.println(" comando(s):");

  for (JsonObject comandoObj : arrayDeComandos) {
    if (!comandoObj.containsKey("comando")) {
      Serial.println("Comando sem o campo 'comando'. Pulando.");
      continue;
    }
    String comando = comandoObj["comando"].as<String>();
    Serial.print("  - Executando comando: ");
    Serial.println(comando);

    if (comando == "reiniciar") {
      Serial.println("    Dispositivo reiniciando por comando remoto...");
      delay(1000); // Pequeno delay para permitir o log
      ESP.restart();
    } else if (comando == "silenciar_buzzer") {
      unsigned long duracao = DURACAO_SILENCIO_BUZZER_MS_PADRAO; // Usa o padrão de sensores.cpp
      if (comandoObj.containsKey("duracao_ms")) {
        duracao = comandoObj["duracao_ms"].as<unsigned long>();
      }
      Serial.print("    Buzzer silenciado por ");
      Serial.print(duracao / 1000);
      Serial.println(" segundos.");
      sensores_silenciar_buzzer_temporariamente(duracao); // Passa a duração correta
    } else if (comando == "reativar_buzzer") {
      Serial.println("    Buzzer reativado por comando remoto.");
      sensores_reativar_buzzer();
    } else {
      Serial.print("    Comando desconhecido: ");
      Serial.println(comando);
    }
  }
}
