// claude-hud: mostra no visor o que o Claude Code está fazendo.
//
// Recebe eventos pela serial USB, um por linha: "<origem> <estado> [id]".
// Comandos extras:
//   ping   responde "pong claude-hud"
//   demo   liga/desliga um passeio por todos os estados
//   shot   devolve o quadro atual em hexadecimal

#include <Arduino.h>

#include "lcd.h"
#include "render.h"
#include "tracker.h"

namespace {

constexpr uint32_t kFrameMs = 40;
constexpr uint32_t kDemoStepMs = 4000;

hud::Tracker tracker;
uint16_t fb[hud::W * hud::H];

char line[64];
size_t line_len = 0;

bool demo = false;
uint32_t demo_start = 0;
uint32_t last_frame = 0;

hud::View demo_view(uint32_t now) {
  static const hud::State states[] = {hud::State::Sleep,   hud::State::Idle,    hud::State::Thinking,
                                      hud::State::Working, hud::State::Waiting, hud::State::Done};
  static const hud::Source sources[] = {hud::Source::Pc, hud::Source::Cloud, hud::Source::Phone};
  const uint32_t elapsed = now - demo_start;
  const uint32_t step = elapsed / kDemoStepMs;
  return hud::View{states[step % 6], sources[(step / 6) % 3], static_cast<uint8_t>(1 + (step / 6) % 3), now,
                   elapsed % kDemoStepMs};
}

void shot() {
  Serial.printf("SHOT %d %d\n", hud::W, hud::H);
  for (int y = 0; y < hud::H; y++) {
    for (int x = 0; x < hud::W; x++) Serial.printf("%04x", fb[y * hud::W + x]);
    Serial.println();
  }
  Serial.println("END");
}

void handle_line(const char* text, uint32_t now) {
  if (strcmp(text, "ping") == 0) {
    Serial.println("pong claude-hud");
  } else if (strcmp(text, "demo") == 0) {
    demo = !demo;
    demo_start = now;
  } else if (strcmp(text, "shot") == 0) {
    shot();
  } else if (tracker.handle(text, now)) {
    demo = false;
  }
  // Linha desconhecida é ignorada em silêncio: se o terminal do PC estiver com
  // eco ligado, qualquer resposta voltaria para cá como um comando novo.
}

void read_serial(uint32_t now) {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      line[line_len] = '\0';
      if (line_len) handle_line(line, now);
      line_len = 0;
    } else if (line_len + 1 < sizeof(line)) {
      line[line_len++] = c;
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(50);
  lcd::begin();
}

void loop() {
  const uint32_t now = millis();
  read_serial(now);

  if (now - last_frame >= kFrameMs) {
    last_frame = now;
    hud::render(demo ? demo_view(now) : tracker.view(now), fb);
    lcd::push(fb);
  }
}
