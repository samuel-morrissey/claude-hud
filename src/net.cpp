#include "net.h"

#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "roots.h"

namespace net {
namespace {

constexpr size_t kEventSize = 48;
constexpr uint32_t kWifiRetryMs = 15000;
constexpr uint32_t kRelayRetryMs = 5000;
// O recadeiro manda um sinal de vida a cada 15 s.
constexpr uint32_t kSilenceMs = 45000;
// Antes desta data o relógio ainda não foi acertado, e sem relógio a validade
// do certificado não pode ser conferida.
constexpr time_t kClockSet = 1700000000;

struct Config {
  String ssid, pass, host, token;
};

QueueHandle_t events = nullptr;
volatile bool reload = true;
volatile bool stream_open = false;
volatile bool has_config = false;

Config load() {
  Preferences prefs;
  Config c;
  if (prefs.begin("hud-net", true)) {
    c.ssid = prefs.getString("ssid", "");
    c.pass = prefs.getString("pass", "");
    c.host = prefs.getString("host", "");
    c.token = prefs.getString("token", "");
    prefs.end();
  }
  return c;
}

void push(const char* event) {
  char item[kEventSize];
  strlcpy(item, event, sizeof(item));
  xQueueSend(events, item, 0);
}

bool wait_for_clock() {
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  for (int i = 0; i < 100 && time(nullptr) < kClockSet; i++) delay(100);
  return time(nullptr) >= kClockSet;
}

// Lê até o fim da linha, sem o terminador. Devolve false se a conexão cair ou
// ficar muda por tempo demais.
bool read_line(WiFiClientSecure& client, char* line, size_t size) {
  size_t len = 0;
  uint32_t last = millis();
  while (!reload) {
    if (client.available()) {
      const char c = static_cast<char>(client.read());
      last = millis();
      if (c == '\n') {
        line[len] = '\0';
        return true;
      }
      if (c != '\r' && len + 1 < size) line[len++] = c;
    } else if (!client.connected() || millis() - last > kSilenceMs) {
      return false;
    } else {
      delay(10);
    }
  }
  return false;
}

// Mantém o fluxo aberto e repassa os eventos até a conexão cair.
void follow(const Config& config) {
  WiFiClientSecure client;
  client.setCACert(kRootCerts);
  if (!client.connect(config.host.c_str(), 443)) return;

  // HTTP/1.0 de propósito: a resposta vem sem "chunked", só linhas.
  client.printf("GET /stream HTTP/1.0\r\nHost: %s\r\nAuthorization: Bearer %s\r\n"
                "Accept: text/event-stream\r\n\r\n",
                config.host.c_str(), config.token.c_str());

  char line[96];
  if (!read_line(client, line, sizeof(line)) || !strstr(line, " 200")) return;
  while (read_line(client, line, sizeof(line)) && line[0] != '\0') {
  }

  stream_open = true;
  while (read_line(client, line, sizeof(line))) {
    if (strncmp(line, "data: ", 6) == 0) push(line + 6);
  }
  stream_open = false;
  // O que a tela mostra deixou de ser confiável.
  push("reset");
}

void task(void*) {
  Config config;
  uint32_t wifi_started = 0;
  for (;;) {
    if (reload) {
      reload = false;
      config = load();
      has_config = config.ssid.length() && config.host.length() && config.token.length();
      WiFi.disconnect(true);
      wifi_started = 0;
    }
    if (!has_config) {
      delay(500);
      continue;
    }
    if (WiFi.status() != WL_CONNECTED) {
      if (wifi_started == 0 || millis() - wifi_started > kWifiRetryMs) {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.begin(config.ssid.c_str(), config.pass.c_str());
        wifi_started = millis() | 1;
      }
      delay(250);
      continue;
    }
    if (time(nullptr) < kClockSet && !wait_for_clock()) continue;

    follow(config);
    for (uint32_t waited = 0; waited < kRelayRetryMs && !reload; waited += 100) delay(100);
  }
}

}  // namespace

void begin() {
  events = xQueueCreate(16, kEventSize);
  xTaskCreate(task, "net", 12288, nullptr, 1, nullptr);
}

bool configured() { return has_config; }

bool online() { return stream_open; }

bool poll(char* out, size_t size) {
  char item[kEventSize];
  if (!events || xQueueReceive(events, item, 0) != pdTRUE) return false;
  strlcpy(out, item, size);
  return true;
}

bool set(const char* key, const char* value) {
  if (strcmp(key, "ssid") && strcmp(key, "pass") && strcmp(key, "host") && strcmp(key, "token")) return false;
  Preferences prefs;
  if (!prefs.begin("hud-net", false)) return false;
  prefs.putString(key, value);
  prefs.end();
  return true;
}

void apply() { reload = true; }

void status(Print& out) {
  const Config c = load();
  out.printf("ssid: %s\n", c.ssid.length() ? c.ssid.c_str() : "(vazio)");
  out.printf("senha: %s\n", c.pass.length() ? "gravada" : "(vazia)");
  out.printf("host: %s\n", c.host.length() ? c.host.c_str() : "(vazio)");
  out.printf("token: %s\n", c.token.length() ? "gravado" : "(vazio)");
  const bool wifi = WiFi.status() == WL_CONNECTED;
  out.printf("wifi: %s\n", wifi ? "conectado" : "desconectado");
  if (wifi) out.printf("ip: %s  sinal: %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  out.printf("relogio: %s\n", time(nullptr) >= kClockSet ? "certo" : "sem hora");
  out.printf("recadeiro: %s\n", stream_open ? "conectado" : "desconectado");
  out.println("fim");
}

}  // namespace net
