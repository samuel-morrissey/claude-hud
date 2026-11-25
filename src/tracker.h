// Guarda o estado de cada sessão do Claude Code e decide qual aparece na tela.
// Sem dependência de Arduino, para ser testado no PC.
#pragma once

#include <stdint.h>

#include "render.h"

namespace hud {

class Tracker {
 public:
  // Linha no formato "<origem> <estado> [id]".
  //   origem: pc | cloud | phone
  //   estado: start | idle | thinking | working | waiting | done | end
  // Devolve false quando a linha não é um evento válido.
  bool handle(const char* line, uint32_t now);

  View view(uint32_t now);

 private:
  struct Session {
    bool used = false;
    char id[12] = "";
    Source source = Source::Pc;
    State state = State::Idle;
    uint32_t since = 0;    // quando entrou no estado atual
    uint32_t updated = 0;  // último evento recebido
  };

  static constexpr int kMax = 8;
  Session sessions_[kMax];

  Session* find(Source source, const char* id);
  Session* allocate(uint32_t now);
  void expire(uint32_t now);
};

}  // namespace hud
