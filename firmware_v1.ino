// PROJETO MOTIVA - CP02 | FIRMWARE 1.0 - Monitoramento de vegetacao com OTA

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

const char *FW_VERSION = "1.0";

const char *WIFI_SSID    = "Wokwi-GUEST";
const char *WIFI_PASS    = "";
const char *MANIFEST_URL =
  "https://raw.githubusercontent.com/Murilo1704/cp02-motiva-ota/main/version.json";

const int PIN_LED_R = 25;
const int PIN_LED_G = 26;
const int PIN_LED_B = 27;

// Laboratorio: 48 h -> 48 s, 2 min -> 2 s
const int           NUM_LEITURAS         = 5;
const unsigned long INTERVALO_LEITURA_MS = 2000UL;
const unsigned long INTERVALO_SESSAO_MS  = 48000UL;
const int           ALTURA_MIN_CM        = 10;
const int           ALTURA_MAX_CM        = 20;
const int           CICLOS_ANTES_OTA     = 3;
const unsigned long WIFI_TIMEOUT_MS      = 10000UL;
const uint16_t      HTTP_TIMEOUT_MS      = 10000;

int           leituras[NUM_LEITURAS];
int           leiturasFeitas   = 0;
unsigned long inicioSessaoMs   = 0;
unsigned long inicioAnteriorMs = 0;
unsigned long numeroSessao     = 0;
bool          sessaoProcessada = false;

void verificarAtualizacao();

// ---------------- LED ----------------
void configurarLed() {
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
}

void definirCorLed(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_R, r ? HIGH : LOW);
  digitalWrite(PIN_LED_G, g ? HIGH : LOW);
  digitalWrite(PIN_LED_B, b ? HIGH : LOW);
}

void ledVersao1() { definirCorLed(false, false, true); }   // azul = FW 1.0

// ---------------- SERIAL ----------------
void imprimirLinha() {
  Serial.println(F("========================================"));
}

void imprimirBanner() {
  Serial.println();
  imprimirLinha();
  Serial.print(F("MONITORAMENTO DE VEGETACAO - FW "));
  Serial.println(FW_VERSION);
  imprimirLinha();
  Serial.println(F("Projeto Motiva | No IoT de campo (simulado)"));
  Serial.print(F("Sessao a cada 48 s | 5 leituras a cada 2 s | OTA a cada "));
  Serial.print(CICLOS_ANTES_OTA);
  Serial.println(F(" sessoes"));
}

// ---------------- MEDICAO ----------------
int gerarLeituraSimulada() {
  return random(ALTURA_MIN_CM, ALTURA_MAX_CM + 1);   // 10 a 20 cm
}

float calcularMedia(const int valores[], int n) {
  long soma = 0;
  for (int i = 0; i < n; i++) soma += valores[i];
  return (float)soma / n;
}

void iniciarSessao(unsigned long agora) {
  inicioAnteriorMs = inicioSessaoMs;
  inicioSessaoMs   = agora;
  leiturasFeitas   = 0;
  sessaoProcessada = false;
  numeroSessao++;

  Serial.println();
  imprimirLinha();
  Serial.print(F("MONITORAMENTO DE VEGETACAO - FW "));
  Serial.println(FW_VERSION);
  imprimirLinha();
  Serial.print(F("Sessao #"));
  Serial.print(numeroSessao);
  Serial.print(F(" | inicio em t = "));
  Serial.print(inicioSessaoMs / 1000.0, 1);
  Serial.print(F(" s"));
  if (numeroSessao > 1) {
    Serial.print(F(" | intervalo desde a sessao anterior: "));
    Serial.print((inicioSessaoMs - inicioAnteriorMs) / 1000.0, 1);
    Serial.print(F(" s"));
  }
  Serial.println();
}

void realizarLeitura() {
  int valor = gerarLeituraSimulada();
  leituras[leiturasFeitas] = valor;
  leiturasFeitas++;

  Serial.print(F("Leitura "));
  Serial.print(leiturasFeitas);
  Serial.print(F(": "));
  Serial.print(valor);
  Serial.print(F(" cm   (t = +"));
  Serial.print((millis() - inicioSessaoMs) / 1000.0, 1);
  Serial.println(F(" s)"));
}

void processarSessao() {
  float media = calcularMedia(leituras, NUM_LEITURAS);
  Serial.print(F("Media da sessao: "));
  Serial.print(media, 1);
  Serial.println(F(" cm"));

  unsigned long decorrido = millis() - inicioSessaoMs;
  unsigned long faltam = (decorrido < INTERVALO_SESSAO_MS)
                         ? (INTERVALO_SESSAO_MS - decorrido) / 1000UL : 0;
  Serial.print(F("Proxima sessao em 48 segundos (contados do inicio desta; faltam "));
  Serial.print(faltam);
  Serial.println(F(" s)."));
}

// Temporizacao com millis(): nova sessao 48 s apos o INICIO da anterior
void atualizarSessao() {
  unsigned long agora = millis();

  if (agora - inicioSessaoMs >= INTERVALO_SESSAO_MS) {
    unsigned long proximoInicio = inicioSessaoMs + INTERVALO_SESSAO_MS;
    if (agora - proximoInicio >= INTERVALO_SESSAO_MS) proximoInicio = agora;
    iniciarSessao(proximoInicio);
  }

  if (leiturasFeitas < NUM_LEITURAS &&
      agora - inicioSessaoMs >= (unsigned long)leiturasFeitas * INTERVALO_LEITURA_MS) {
    realizarLeitura();
  }

  if (leiturasFeitas == NUM_LEITURAS && !sessaoProcessada) {
    sessaoProcessada = true;
    processarSessao();
    if (numeroSessao % CICLOS_ANTES_OTA == 0) {   // verifica OTA a cada 3 ciclos
      verificarAtualizacao();
    }
  }
}

// ---------------- WI-FI ----------------
void iniciarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);
  Serial.print(F("[WIFI] Conectando a rede "));
  Serial.print(WIFI_SSID);
  Serial.println(F(" em segundo plano..."));
}

bool garantirWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.println(F("[WIFI] Sem conexao. Tentando conectar..."));
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < WIFI_TIMEOUT_MS) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("[WIFI] Conectado. IP: "));
    Serial.println(WiFi.localIP());
    return true;
  }
  return false;
}

// ---------------- OTA ----------------
// "2.0" -> 2000, "1.0" -> 1000, para comparar versoes
long versaoParaNumero(const String &versao) {
  int maior = 0, menor = 0;
  sscanf(versao.c_str(), "%d.%d", &maior, &menor);
  return (long)maior * 1000L + menor;
}

bool lerManifesto(String &versaoRemota, String &urlFirmware) {
  WiFiClientSecure cliente;
  cliente.setInsecure();

  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  Serial.print(F("[OTA] Consultando manifesto: "));
  Serial.println(MANIFEST_URL);

  if (!http.begin(cliente, MANIFEST_URL)) {
    Serial.println(F("[OTA] ERRO: manifesto nao pode ser acessado (URL invalida)."));
    return false;
  }

  int codigo = http.GET();
  if (codigo != HTTP_CODE_OK) {
    Serial.print(F("[OTA] ERRO: manifesto nao pode ser acessado. "));
    if (codigo > 0) {
      Serial.print(F("Servidor respondeu HTTP "));
      Serial.println(codigo);
    } else {
      Serial.print(F("Falha de conexao: "));
      Serial.println(http.errorToString(codigo));
    }
    http.end();
    return false;
  }

  String corpo = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, corpo);
  if (erro) {
    Serial.print(F("[OTA] ERRO: manifesto invalido (JSON): "));
    Serial.println(erro.c_str());
    return false;
  }

  versaoRemota = doc["version"] | "";
  urlFirmware  = doc["url"] | "";
  if (versaoRemota.length() == 0 || urlFirmware.length() == 0) {
    Serial.println(F("[OTA] ERRO: manifesto sem os campos \"version\" e/ou \"url\"."));
    return false;
  }

  Serial.print(F("[OTA] Manifesto lido: versao disponivel = "));
  Serial.print(versaoRemota);
  Serial.print(F(" | url = "));
  Serial.println(urlFirmware);
  return true;
}

void aoIniciarOta() {
  Serial.println(F("[OTA] Download iniciado. Gravando na particao OTA livre..."));
}

void aoProgredirOta(int atual, int total) {
  static int ultimoPct = -1;
  if (total <= 0) return;
  int pct = (int)((atual * 100LL) / total);
  if (pct / 10 != ultimoPct / 10) {
    ultimoPct = pct;
    Serial.printf("[OTA] Progresso: %3d%% (%d / %d bytes)\n", pct, atual, total);
    definirCorLed(false, false, (pct / 10) % 2 == 0);
  }
}

// Separa falha de download de falha na gravacao/validacao
bool erroEhDeDownload(int codigo) {
  return codigo == HTTP_UE_SERVER_NOT_REPORT_SIZE ||
         codigo == HTTP_UE_SERVER_FILE_NOT_FOUND  ||
         codigo == HTTP_UE_SERVER_FORBIDDEN       ||
         codigo == HTTP_UE_SERVER_WRONG_HTTP_CODE ||
         (codigo < 0 && codigo > HTTP_UE_TOO_LESS_SPACE);
}

void executarOta(const String &urlFirmware) {
  WiFiClientSecure cliente;
  cliente.setInsecure();
  cliente.setTimeout(HTTP_TIMEOUT_MS / 1000);

  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  httpUpdate.rebootOnUpdate(false);
  httpUpdate.setLedPin(-1);
  httpUpdate.onStart(aoIniciarOta);
  httpUpdate.onProgress(aoProgredirOta);

  Serial.print(F("[OTA] Baixando firmware: "));
  Serial.println(urlFirmware);

  t_httpUpdate_return resultado = httpUpdate.update(cliente, urlFirmware);

  switch (resultado) {
    case HTTP_UPDATE_OK:
      Serial.println(F("[OTA] SUCESSO: firmware gravado e verificado."));
      Serial.println(F("[OTA] Reiniciando o ESP32 para executar a nova versao..."));
      Serial.flush();
      delay(1000);
      ESP.restart();
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println(F("[OTA] Servidor informou que nao ha atualizacao."));
      break;

    case HTTP_UPDATE_FAILED: {
      int codigo = httpUpdate.getLastError();
      if (erroEhDeDownload(codigo)) {
        Serial.print(F("[OTA] ERRO: arquivo de firmware nao pode ser baixado ("));
      } else {
        Serial.print(F("[OTA] ERRO: o processo de atualizacao retornou erro ("));
      }
      Serial.print(codigo);
      Serial.print(F(": "));
      Serial.print(httpUpdate.getLastErrorString());
      Serial.println(F(")."));
      Serial.println(F("[OTA] O firmware atual continua em execucao. Nova tentativa no proximo ciclo de verificacao."));
      break;
    }
  }
  ledVersao1();
}

void verificarAtualizacao() {
  Serial.println();
  Serial.println(F("----------------------------------------"));
  Serial.print(F("[OTA] "));
  Serial.print(numeroSessao);
  Serial.println(F(" ciclos concluidos. Verificando atualizacao remota..."));

  if (!garantirWiFi()) {
    Serial.println(F("[OTA] ERRO: nao ha conexao Wi-Fi. Verificacao adiada para o proximo ciclo."));
    Serial.println(F("----------------------------------------"));
    return;
  }

  String versaoRemota, urlFirmware;
  if (!lerManifesto(versaoRemota, urlFirmware)) {
    Serial.println(F("[OTA] Mantendo o firmware atual. Nova tentativa no proximo ciclo."));
    Serial.println(F("----------------------------------------"));
    return;
  }

  Serial.print(F("[OTA] Versao instalada: "));
  Serial.print(FW_VERSION);
  Serial.print(F(" | versao disponivel: "));
  Serial.println(versaoRemota);

  if (versaoParaNumero(versaoRemota) <= versaoParaNumero(FW_VERSION)) {
    Serial.println(F("[OTA] A versao instalada ja e a mais recente. Nenhuma acao necessaria."));
    Serial.println(F("----------------------------------------"));
    return;
  }

  Serial.println(F("[OTA] ATUALIZACAO DISPONIVEL! Iniciando OTA..."));
  executarOta(urlFirmware);
  Serial.println(F("----------------------------------------"));
}

// ---------------- SETUP / LOOP ----------------
void setup() {
  Serial.begin(115200);
  delay(300);
  configurarLed();
  ledVersao1();
  randomSeed(esp_random());

  imprimirBanner();
  iniciarWiFi();

  iniciarSessao(millis());
}

void loop() {
  atualizarSessao();
}
