// Wi-Fi e conexão com o recadeiro. Roda em uma tarefa própria, para que a
// animação não pare enquanto a rede conecta.
#pragma once

#include <Arduino.h>

namespace net {

// Lê a configuração gravada e, se houver, começa a conectar.
void begin();

// Há rede e recadeiro configurados?
bool configured();

// O fluxo de eventos do recadeiro está aberto?
bool online();

// Copia para out o próximo evento recebido. Devolve false quando não há.
bool poll(char* out, size_t size);

// Grava um item da configuração: ssid, pass, host ou token. Vale a partir do
// próximo apply(). Devolve false para um item desconhecido.
bool set(const char* key, const char* value);

// Reconecta com a configuração gravada.
void apply();

// Descreve o estado da rede, sem revelar senha nem segredo.
void status(Print& out);

}  // namespace net
