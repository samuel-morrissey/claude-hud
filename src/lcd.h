#pragma once

#include <stdint.h>

namespace lcd {

void begin();

// Envia um quadro inteiro (hud::W * hud::H pixels RGB565, ordem nativa).
void push(const uint16_t* fb);

}  // namespace lcd
