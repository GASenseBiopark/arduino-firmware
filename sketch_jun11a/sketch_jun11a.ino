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
#define RESET_WIFI_PIN 25

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
const unsigned long INTERVALO_NORMAL = 15000;  // 15s
const unsigned long INTERVALO_ALERTA = 1000;   // 1s
unsigned long ultimoEnvio = 0;

// Limites
const int LIMITE_MQ2 = 600;
const int LIMITE_MQ4 = 600;
const int LIMITE_MQ135 = 600;
const bool LIMITE_CHAMA = true;  // true = fogo detectado

// Configuração do servidor NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -10800;  // UTC-3 (horário de Brasília)
const int daylightOffset_sec = 0;   // Sem horário de verão

// Função para carregar WiFi salvo do SPIFFS
bool carregarCredenciais() {
  if (!SPIFFS.begin(true)) {
    Serial.println("Erro ao montar SPIFFS");
    return false;
  }
  if (!SPIFFS.exists("/wifi.json")) {
    Serial.println("Arquivo de WiFi não existe");
    return false;
  }
  File file = SPIFFS.open("/wifi.json", "r");
  if (!file) {
    Serial.println("Erro ao abrir arquivo de WiFi");
    return false;
  }
  size_t size = file.size();
  std::unique_ptr<char[]> buf(new char[size]);
  file.readBytes(buf.get(), size);
  file.close();

  StaticJsonDocument<256> doc;
  auto err = deserializeJson(doc, buf.get());
  if (err) {
    Serial.println("Erro ao desserializar JSON");
    return false;
  }
  ssid = String((const char*)doc["ssid"]);
  password = String((const char*)doc["password"]);
  Serial.printf("Credenciais carregadas: SSID=%s\n", ssid.c_str());
  Serial.print("Conectando a SSID: '");
  Serial.print(ssid);
  Serial.println("'");
  Serial.print("Com senha: '");
  Serial.print(password);
  Serial.println("'");
  return true;
}

// Salvar credenciais na SPIFFS
void salvarCredenciais(String newSsid, String newPass) {
  StaticJsonDocument<256> doc;
  doc["ssid"] = newSsid;
  doc["password"] = newPass;

  File file = SPIFFS.open("/wifi.json", "w");
  if (!file) {
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
  int numRedes = WiFi.scanNetworks();
  String options = "";

  for (int i = 0; i < numRedes; i++) {
    String ssid = WiFi.SSID(i);
    options += "<option value='" + ssid + "'>" + ssid + "</option>";
  }

  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="pt-BR">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Configurar WiFi</title>
    <style>
      * {
        box-sizing: border-box;
        margin: 0;
        padding: 0;
      }
      body {
        font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        background: #f0f2f5;
        height: 100vh;
        display: flex;
        justify-content: center;
        align-items: center;
        padding: 1rem;
      }
      .card {
        background: white;
        padding: 2rem;
        border-radius: 24px;
        box-shadow: 0 8px 24px rgba(0, 0, 0, 0.1);
        width: 100%;
        max-width: 420px;
      }
      h1 {
        font-size: 1.6rem;
        margin-bottom: 1.5rem;
        color: #333;
        text-align: center;
      }
      select,
      input[type='text'],
      input[type='password'] {
        width: 100%;
        padding: 0.85rem;
        margin-bottom: 1rem;
        border: 1px solid #ccc;
        border-radius: 18px;
        font-size: 1rem;
      }
      input[type='submit'] {
        width: 100%;
        padding: 0.85rem;
        background-color: #007BFF;
        color: white;
        font-size: 1rem;
        border: none;
        border-radius: 20px;
        cursor: pointer;
      }
      input[type='submit']:hover {
        background-color: #0056b3;
      }
      @media (max-width: 480px) {
        .card {
          padding: 1.5rem;
          border-radius: 20px;
        }
        h1 {
          font-size: 1.4rem;
        }
        select,
        input[type='text'],
        input[type='password'],
        input[type='submit'] {
          font-size: 0.95rem;
          padding: 0.75rem;
        }
      }
    </style>
    <script>
      function toggleInput(select) {
        const outroInput = document.getElementById('ssid_manual');
        if (select.value === 'outro') {
          outroInput.style.display = 'block';
          outroInput.required = true;
        } else {
          outroInput.style.display = 'none';
          outroInput.required = false;
        }
      }
    </script>
  </head>
  <body>
    <div class="card">
      <h1>Configurar Wi-Fi</h1>
      <form action="/salvar" method="POST">
        <select name="ssid" onchange="toggleInput(this)" required>
  )rawliteral";

  html += options;
  html += "<option value='outro'>Outro...</option>";

  html += R"rawliteral(
        </select>
        <input type="text" id="ssid_manual" name="ssid" placeholder="Digite o SSID" style="display: none;">
        <input type="password" name="password" placeholder="Senha" required>
        <input type="submit" value="Salvar">
      </form>
    </div>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", html);
}



// Rota para salvar credenciais
void salvarWiFi() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String newSsid = server.arg("ssid");
    String newPass = server.arg("password");

    salvarCredenciais(newSsid, newPass);

    String resposta = R"rawliteral(
    <!DOCTYPE html>
    <html lang="pt-BR">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Salvo com Sucesso</title>
      <style>
        * {
          box-sizing: border-box;
          margin: 0;
          padding: 0;
        }
        body {
          font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
          background: #f0f2f5;
          height: 100vh;
          display: flex;
          justify-content: center;
          align-items: center;
          padding: 1rem;
        }
        .card {
          background: white;
          padding: 2rem;
          border-radius: 24px;
          box-shadow: 0 8px 24px rgba(0, 0, 0, 0.1);
          width: 100%;
          max-width: 420px;
          text-align: center;
        }
        h1 {
          font-size: 1.6rem;
          margin-bottom: 1rem;
          color: #28a745;
        }
        p {
          font-size: 1rem;
          color: #333;
        }
      </style>
    </head>
    <body>
      <div class="card">
        <h1>Credenciais salvas!</h1>
        <p>Reinicie o dispositivo para conectar à rede Wi-Fi.</p>
      </div>
    </body>
    </html>
    )rawliteral";

    server.send(200, "text/html", resposta);
    delay(3000);  // Tempo para o usuário ver a mensagem
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Dados inválidos");
  }
}


// Tenta conectar WiFi com SSID e senha salvos
bool conectarWiFi() {
  if (ssid.length() == 0) {
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
  if (apOk) {
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
  pinMode(RESET_WIFI_PIN, INPUT_PULLUP);
  dht.begin();

  // Tenta carregar credenciais
  if (!carregarCredenciais() || !conectarWiFi()) {
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
    server.handleClient();  // Necessário para processar requisições web
  }

  if (digitalRead(RESET_WIFI_PIN) == LOW) {
    delay(50);  // debounce
    if (digitalRead(RESET_WIFI_PIN) == LOW) {
      SPIFFS.begin(true);
      if (SPIFFS.exists("/wifi.json")) {
        SPIFFS.remove("/wifi.json");
        Serial.println("Wi-Fi resetado por botão.");
      }
      while (digitalRead(RESET_WIFI_PIN) == LOW)
        ;  // espera soltar
      delay(1000);
      ESP.restart();
    }
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
      http.setReuse(false);
      http.begin(apiEndpoint);
      http.setReuse(false);
      http.setTimeout(10000); 
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

      if (response > 0) {
        Serial.print("Código de resposta: ");
        Serial.println(response);
      } else {
        Serial.printf("Erro HTTP: %d -> %s\n", response, http.errorToString(response).c_str());
      }

      http.end();
      delay(5000);
    }
    ultimoEnvio = agora;
  }
}