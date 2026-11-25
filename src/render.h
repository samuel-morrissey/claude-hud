// Desenho da tela. Sem dependência de Arduino: compila igual na placa e no PC,
// onde tools/preview gera as imagens de conferência.
#pragma once

#include <stdint.h>

namespace hud {

constexpr int W = 96;
constexpr int H = 54;

enum class State : uint8_t { Sleep, Idle, Thinking, Working, Waiting, Done };
enum class Source : uint8_t { Pc, Cloud, Phone };

struct View {
  State state;
  Source source;
  uint8_t sessions;  // sessões abertas, somando todas as origens
  uint32_t now;      // relógio livre, em ms
  uint32_t since;    // ms desde que este estado começou
};

// Preenche fb (W*H pixels RGB565, ordem nativa) com o quadro de v.
void render(const View& v, uint16_t* fb);

}  // namespace hud
