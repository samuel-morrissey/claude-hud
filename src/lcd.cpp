#include "lcd.h"

#include <Arduino.h>
#include <esp32-hal-spi.h>

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
spi_t* bus = nullptr;

void command(uint8_t cmd, const uint8_t* data = nullptr, size_t len = 0) {
  digitalWrite(LCD_PIN_DC, LOW);
  spiWriteNL(bus, &cmd, 1);
  digitalWrite(LCD_PIN_DC, HIGH);
  if (len) spiWriteNL(bus, data, len);
}

void window(int x0, int y0, int x1, int y1) {
  const uint8_t cols[4] = {0, static_cast<uint8_t>(x0), 0, static_cast<uint8_t>(x1)};
  const uint8_t rows[4] = {0, static_cast<uint8_t>(y0), 0, static_cast<uint8_t>(y1)};
  command(CASET, cols, 4);
  command(RASET, rows, 4);
  command(RAMWR);
}

// Sem CS, o visor nunca ressincroniza: uma única borda falsa no relógio
// desalinha os bits até a próxima vez que ele for desligado. Por isso o
// relógio já sobe antes de virar saída, e o barramento faz uma transferência
// no vazio, que fixa o repouso em nível alto, antes de assumir os pinos.
void start_bus() {
  digitalWrite(LCD_PIN_SCL, HIGH);
  pinMode(LCD_PIN_SCL, OUTPUT);
  pinMode(LCD_PIN_DC, OUTPUT);
  digitalWrite(LCD_PIN_DC, HIGH);

  bus = spiStartBus(FSPI, spiFrequencyToClockDiv(LCD_SPI_HZ), SPI_MODE3, SPI_MSBFIRST);
  const uint8_t idle = 0;
  spiSimpleTransaction(bus);
  spiWriteNL(bus, &idle, 1);
  spiEndTransaction(bus);

  spiAttachSCK(bus, LCD_PIN_SCL);
  spiAttachMOSI(bus, LCD_PIN_SDA);
}

}  // namespace

void begin() {
  start_bus();
  delay(200);

  spiSimpleTransaction(bus);
  command(SWRESET);
  spiEndTransaction(bus);
  delay(150);
  spiSimpleTransaction(bus);
  command(SLPOUT);
  spiEndTransaction(bus);
  delay(120);

  spiSimpleTransaction(bus);
  const uint8_t colmod = 0x05;  // 16 bits por pixel
  command(COLMOD, &colmod, 1);
  const uint8_t madctl = (LCD_MIRROR & 0xC0) | (LCD_BGR ? 0x08 : 0x00);
  command(MADCTL, &madctl, 1);
  command(LCD_INVERT ? INVON : INVOFF);

  // Apaga a memória inteira: fora do recorte visível sobra lixo do reset.
  window(0, 0, RAM_COLS - 1, RAM_ROWS - 1);
  memset(line, 0, sizeof(line));
  for (int left = RAM_COLS * RAM_ROWS * 2; left > 0; left -= sizeof(line)) {
    spiWriteNL(bus, line, left < static_cast<int>(sizeof(line)) ? left : sizeof(line));
  }

  command(NORON);
  spiEndTransaction(bus);
  delay(10);
  spiSimpleTransaction(bus);
  command(DISPON);
  spiEndTransaction(bus);
  delay(20);
}

void push(const uint16_t* fb) {
  spiSimpleTransaction(bus);
  window(LCD_COL_OFFSET, LCD_ROW_OFFSET, LCD_COL_OFFSET + hud::W - 1, LCD_ROW_OFFSET + hud::H - 1);
  for (int y = 0; y < hud::H; y++) {
    for (int x = 0; x < hud::W; x++) {
      const int i = y * hud::W + x;
      const uint16_t c = fb[LCD_ROTATE_180 ? hud::W * hud::H - 1 - i : i];
      line[x * 2] = c >> 8;
      line[x * 2 + 1] = c & 0xFF;
    }
    spiWriteNL(bus, line, sizeof(line));
  }
  spiEndTransaction(bus);
}

}  // namespace lcd
