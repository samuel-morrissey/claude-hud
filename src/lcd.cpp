#include "lcd.h"

#include <Arduino.h>
#include <SPI.h>

#include "config.h"
#include "render.h"

namespace lcd {
namespace {

constexpr uint8_t SWRESET = 0x01;
constexpr uint8_t SLPOUT = 0x11;
constexpr uint8_t NORON = 0x13;
constexpr uint8_t INVOFF = 0x20;
constexpr uint8_t INVON = 0x21;
constexpr uint8_t DISPON = 0x29;
constexpr uint8_t CASET = 0x2A;
constexpr uint8_t RASET = 0x2B;
constexpr uint8_t RAMWR = 0x2C;
constexpr uint8_t MADCTL = 0x36;
constexpr uint8_t COLMOD = 0x3A;

constexpr int RAM_COLS = 128;
constexpr int RAM_ROWS = 160;

uint8_t line[hud::W * 2];

void command(uint8_t cmd, const uint8_t* data = nullptr, size_t len = 0) {
  digitalWrite(LCD_PIN_DC, LOW);
  SPI.transfer(cmd);
  digitalWrite(LCD_PIN_DC, HIGH);
  if (len) SPI.writeBytes(data, len);
}

void window(int x0, int y0, int x1, int y1) {
  const uint8_t cols[4] = {0, static_cast<uint8_t>(x0), 0, static_cast<uint8_t>(x1)};
  const uint8_t rows[4] = {0, static_cast<uint8_t>(y0), 0, static_cast<uint8_t>(y1)};
  command(CASET, cols, 4);
  command(RASET, rows, 4);
  command(RAMWR);
}

void begin_bus() {
  SPI.beginTransaction(SPISettings(LCD_SPI_HZ, MSBFIRST, SPI_MODE3));
}

}  // namespace

void begin() {
  pinMode(LCD_PIN_DC, OUTPUT);
  digitalWrite(LCD_PIN_DC, HIGH);
  SPI.begin(LCD_PIN_SCL, -1, LCD_PIN_SDA, -1);

  delay(200);
  begin_bus();
  command(SWRESET);
  delay(150);
  command(SLPOUT);
  delay(120);

  const uint8_t colmod = 0x05;  // 16 bits por pixel
  command(COLMOD, &colmod, 1);
  const uint8_t madctl = (LCD_MIRROR & 0xC0) | (LCD_BGR ? 0x08 : 0x00);
  command(MADCTL, &madctl, 1);
  command(LCD_INVERT ? INVON : INVOFF);

  // Apaga a memória inteira: fora do recorte visível sobra lixo do reset.
  window(0, 0, RAM_COLS - 1, RAM_ROWS - 1);
  memset(line, 0, sizeof(line));
  for (int sent = 0; sent < RAM_COLS * RAM_ROWS * 2; sent += sizeof(line)) {
    SPI.writeBytes(line, sizeof(line));
  }

  command(NORON);
  delay(10);
  command(DISPON);
  delay(20);
  SPI.endTransaction();
}

void push(const uint16_t* fb) {
  begin_bus();
  window(LCD_COL_OFFSET, LCD_ROW_OFFSET, LCD_COL_OFFSET + hud::W - 1, LCD_ROW_OFFSET + hud::H - 1);
  for (int y = 0; y < hud::H; y++) {
    for (int x = 0; x < hud::W; x++) {
      const uint16_t c = fb[y * hud::W + x];
      line[x * 2] = c >> 8;
      line[x * 2 + 1] = c & 0xFF;
    }
    SPI.writeBytes(line, sizeof(line));
  }
  SPI.endTransaction();
}

}  // namespace lcd
