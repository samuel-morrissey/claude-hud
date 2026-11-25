// Testes do Tracker, rodados no PC: make test

#include <cstdio>
#include <cstdlib>

#include "../src/tracker.h"

using namespace hud;

static int failures = 0;

#define CHECK(cond)                                                \
  do {                                                             \
    if (!(cond)) {                                                 \
      std::printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                                  \
    }                                                              \
  } while (0)

constexpr uint32_t kSecond = 1000;
constexpr uint32_t kMinute = 60 * kSecond;

static void sem_sessoes_dorme() {
  Tracker t;
  const View v = t.view(0);
  CHECK(v.state == State::Sleep);
  CHECK(v.sessions == 0);
}

static void segue_o_ciclo_de_uma_sessao() {
  Tracker t;
  CHECK(t.handle("pc start abc", 0));
  CHECK(t.view(1).state == State::Idle);
  CHECK(t.handle("pc thinking abc", 10));
  CHECK(t.view(11).state == State::Thinking);
  CHECK(t.handle("pc working abc", 20));
  CHECK(t.view(21).state == State::Working);
  CHECK(t.handle("pc done abc", 30));
  CHECK(t.view(31).state == State::Done);
  CHECK(t.view(31).sessions == 1);
  CHECK(t.handle("pc end abc", 40));
  CHECK(t.view(41).state == State::Sleep);
}

static void since_conta_desde_a_troca_de_estado() {
  Tracker t;
  t.handle("pc working abc", 1000);
  t.handle("pc working abc", 3000);
  CHECK(t.view(4000).since == 3000);
}

static void comemoracao_acaba_e_depois_dorme() {
  Tracker t;
  t.handle("pc done abc", 0);
  CHECK(t.view(19 * kSecond).state == State::Done);
  CHECK(t.view(21 * kSecond).state == State::Idle);
  CHECK(t.view(21 * kSecond + 6 * kMinute).state == State::Sleep);
}

static void mostra_a_sessao_mais_urgente() {
  Tracker t;
  t.handle("pc working a", 0);
  t.handle("cloud waiting b", 10);
  t.handle("phone thinking c", 20);
  const View v = t.view(30);
  CHECK(v.state == State::Waiting);
  CHECK(v.source == Source::Cloud);
  CHECK(v.sessions == 3);

  t.handle("cloud end b", 40);
  CHECK(t.view(50).state == State::Working);
  CHECK(t.view(50).source == Source::Pc);
}

static void mesmo_id_em_origens_diferentes_sao_sessoes_distintas() {
  Tracker t;
  t.handle("pc working a", 0);
  t.handle("cloud working a", 0);
  CHECK(t.view(1).sessions == 2);
}

static void sessao_sem_eventos_expira() {
  Tracker t;
  t.handle("pc working a", 0);
  CHECK(t.view(29 * kMinute).state == State::Working);
  CHECK(t.view(31 * kMinute).state == State::Sleep);
  CHECK(t.view(31 * kMinute).sessions == 0);
}

static void tabela_cheia_descarta_a_mais_antiga() {
  Tracker t;
  char line[32];
  for (int i = 0; i < 9; i++) {
    std::snprintf(line, sizeof(line), "pc working s%d", i);
    t.handle(line, i * 10);
  }
  CHECK(t.view(100).sessions == 8);
  // s0 saiu: encerrar de novo não muda a contagem.
  t.handle("pc end s0", 110);
  CHECK(t.view(120).sessions == 8);
  t.handle("pc end s8", 130);
  CHECK(t.view(140).sessions == 7);
}

static void recusa_linhas_invalidas() {
  Tracker t;
  CHECK(!t.handle("", 0));
  CHECK(!t.handle("pc", 0));
  CHECK(!t.handle("tv working a", 0));
  CHECK(!t.handle("pc dancing a", 0));
  CHECK(!t.handle("HUD ST7735 96x54", 0));
  CHECK(t.view(1).sessions == 0);
}

static void aceita_linha_sem_id_e_com_fim_de_linha() {
  Tracker t;
  CHECK(t.handle("pc working\r\n", 0));
  CHECK(t.handle("pc done", 10));
  CHECK(t.view(11).sessions == 1);
  CHECK(t.view(11).state == State::Done);
}

static void reset_esquece_todas_as_sessoes() {
  Tracker t;
  t.handle("pc working a", 0);
  t.handle("cloud waiting b", 0);
  CHECK(t.handle("reset", 10));
  CHECK(t.view(20).sessions == 0);
  CHECK(t.view(20).state == State::Sleep);
}

int main() {
  sem_sessoes_dorme();
  segue_o_ciclo_de_uma_sessao();
  since_conta_desde_a_troca_de_estado();
  comemoracao_acaba_e_depois_dorme();
  mostra_a_sessao_mais_urgente();
  mesmo_id_em_origens_diferentes_sao_sessoes_distintas();
  sessao_sem_eventos_expira();
  tabela_cheia_descarta_a_mais_antiga();
  recusa_linhas_invalidas();
  aceita_linha_sem_id_e_com_fim_de_linha();
  reset_esquece_todas_as_sessoes();

  if (failures) {
    std::printf("%d falha(s)\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("ok\n");
  return EXIT_SUCCESS;
}
