#include "render.h"

namespace hud {
namespace {

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kOrange = rgb(217, 119, 87);
constexpr uint16_t kOrangeDim = rgb(120, 62, 44);
constexpr uint16_t kWhite = rgb(240, 236, 228);
constexpr uint16_t kGray = rgb(130, 126, 120);
constexpr uint16_t kYellow = rgb(255, 208, 64);
constexpr uint16_t kGreen = rgb(110, 200, 120);
constexpr uint16_t kBlue = rgb(110, 170, 240);

// Lado de um "pixel" do boneco, em pixels da tela.
constexpr int S = 3;

uint16_t* fb = nullptr;

void px(int x, int y, uint16_t c) {
  if (x >= 0 && x < W && y >= 0 && y < H) fb[y * W + x] = c;
}

void rect(int x, int y, int w, int h, uint16_t c) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) px(x + i, y + j, c);
}

void frame(int x, int y, int w, int h, uint16_t c) {
  rect(x, y, w, 1, c);
  rect(x, y + h - 1, w, 1, c);
  rect(x, y, 1, h, c);
  rect(x + w - 1, y, 1, h, c);
}

void disc(int cx, int cy, int r, uint16_t c) {
  for (int j = -r; j <= r; j++)
    for (int i = -r; i <= r; i++)
      if (i * i + j * j <= r * r + r / 2) px(cx + i, cy + j, c);
}

uint32_t hash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

enum class Eyes { Open, Closed, Happy };

struct Pose {
  int dx = 0;
  int dy = 0;
  Eyes eyes = Eyes::Open;
  int eyeDx = 0;
  int eyeDy = 0;
  bool armLeftUp = false;
  bool armRightUp = false;
  int legPhase = 0;  // 0 parado, 1 e 2 alternam os pares de pernas
  uint16_t body = kOrange;
};

// Boneco em uma grade de 14x9, com o canto superior esquerdo em (x0, y0).
void mascot(int x0, int y0, const Pose& p) {
  x0 += p.dx;
  y0 += p.dy;

  rect(x0 + 2 * S, y0, 10 * S, 7 * S, p.body);
  rect(x0, y0 + (p.armLeftUp ? 1 : 3) * S, 2 * S, 2 * S, p.body);
  rect(x0 + 12 * S, y0 + (p.armRightUp ? 1 : 3) * S, 2 * S, 2 * S, p.body);

  const int legCols[4] = {2, 4, 9, 11};
  for (int i = 0; i < 4; i++) {
    const bool lifted = p.legPhase != 0 && ((i % 2 == 0) == (p.legPhase == 1));
    rect(x0 + legCols[i] * S, y0 + 7 * S, S, lifted ? S : 2 * S, p.body);
  }

  const int eyeCols[2] = {4, 9};
  for (int col : eyeCols) {
    const int ex = x0 + col * S + p.eyeDx;
    const int ey = y0 + 2 * S + p.eyeDy;
    switch (p.eyes) {
      case Eyes::Open:
        rect(ex, ey, S, 2 * S, kBlack);
        break;
      case Eyes::Closed:
        rect(ex - 1, ey + S + 1, S + 2, 2, kBlack);
        break;
      case Eyes::Happy:
        for (int i = 0; i < 5; i++) {
          const int rise = i <= 2 ? i : 4 - i;
          rect(ex - 1 + i, ey + 4 - rise, 1, 2, kBlack);
        }
        break;
    }
  }
}

void letterZ(int x, int y, int n, uint16_t c) {
  rect(x, y, n, 1, c);
  rect(x, y + n - 1, n, 1, c);
  for (int i = 0; i < n; i++) px(x + n - 1 - i, y + i, c);
}

void iconPc(int x, int y, uint16_t c) {
  frame(x, y, 22, 14, c);
  frame(x + 1, y + 1, 20, 12, c);
  rect(x + 9, y + 14, 4, 3, c);
  rect(x + 5, y + 17, 12, 2, c);
}

void iconCloud(int x, int y, uint16_t c) {
  disc(x + 6, y + 11, 5, c);
  disc(x + 12, y + 7, 7, c);
  disc(x + 18, y + 11, 5, c);
  rect(x + 6, y + 11, 13, 6, c);
}

void iconPhone(int x, int y, uint16_t c) {
  frame(x + 5, y, 12, 19, c);
  frame(x + 6, y + 1, 10, 17, c);
  rect(x + 9, y + 15, 4, 1, c);
}

// Dígitos 3x5, um bit por pixel, linha de cima primeiro.
const uint16_t kDigits[10] = {
    0b111101101101111, 0b010110010010111, 0b111001111100111, 0b111001111001111,
    0b101101111001001, 0b111100111001111, 0b111100111101111, 0b111001001001001,
    0b111101111101111, 0b111101111001111,
};

void digit(int x, int y, int d, int scale, uint16_t c) {
  for (int j = 0; j < 5; j++)
    for (int i = 0; i < 3; i++)
      if (kDigits[d] >> (14 - (j * 3 + i)) & 1) rect(x + i * scale, y + j * scale, scale, scale, c);
}

void number(int cx, int y, int n, uint16_t c) {
  if (n > 99) n = 99;
  const int scale = 2;
  const int digits = n >= 10 ? 2 : 1;
  int x = cx - (digits * 4 * scale - scale) / 2;
  if (digits == 2) {
    digit(x, y, n / 10, scale, c);
    x += 4 * scale;
  }
  digit(x, y, n % 10, scale, c);
}

void sidebar(const View& v) {
  if (v.state == State::Sleep) return;

  const int x = 67;
  const int y = v.sessions > 1 ? 9 : 17;
  switch (v.source) {
    case Source::Pc:
      iconPc(x, y, kWhite);
      break;
    case Source::Cloud:
      iconCloud(x, y, kBlue);
      break;
    case Source::Phone:
      iconPhone(x, y, kGreen);
      break;
  }
  if (v.sessions > 1) number(x + 11, y + 25, v.sessions, kGray);
}

void confetti(uint32_t since) {
  const uint16_t palette[4] = {kYellow, kGreen, kBlue, kWhite};
  for (uint32_t i = 0; i < 16; i++) {
    const uint32_t h = hash(i + 1);
    const int x = static_cast<int>(h % W);
    const uint32_t speed = 22 + (h >> 8) % 20;  // ms por pixel
    const int y = static_cast<int>((since / speed + (h >> 16)) % (H + 10)) - 5;
    rect(x, y, 2, 2, palette[i % 4]);
  }
}

}  // namespace

void render(const View& v, uint16_t* out) {
  fb = out;
  rect(0, 0, W, H, kBlack);

  const int x0 = 10;
  const int y0 = 17;
  Pose p;

  switch (v.state) {
    case State::Sleep: {
      p.body = kOrangeDim;
      p.eyes = Eyes::Closed;
      p.dy = (v.now / 1500) % 2;
      const int step = static_cast<int>((v.now / 600) % 4);
      if (step >= 1) letterZ(x0 + 57, y0 - 2, 4, kGray);
      if (step >= 2) letterZ(x0 + 63, y0 - 9, 5, kGray);
      if (step >= 3) letterZ(x0 + 70, y0 - 17, 6, kWhite);
      mascot(x0 + 12, y0, p);
      return;
    }
    case State::Idle: {
      p.dy = (v.now / 1200) % 2;
      if (v.now % 4000 < 160) p.eyes = Eyes::Closed;
      break;
    }
    case State::Thinking: {
      const int look = static_cast<int>((v.now / 700) % 4);
      p.eyeDx = look == 1 ? 2 : look == 3 ? -2 : 0;
      p.eyeDy = -2;
      const int lit = static_cast<int>((v.now / 350) % 4);
      for (int i = 0; i < 3; i++)
        rect(x0 + 12 + i * 7, y0 - 9, 4, 4, i < lit ? kWhite : kGray);
      break;
    }
    case State::Working: {
      const int step = static_cast<int>((v.now / 110) % 2);
      p.legPhase = step + 1;
      p.dy = step;
      for (int i = 0; i < 3; i++) {
        const int offset = static_cast<int>((v.now / 40 + i * 5) % 12);
        rect(8 - offset, y0 + 4 + i * 8, 4, 1, kGray);
      }
      break;
    }
    case State::Waiting: {
      const int t = static_cast<int>(v.now % 800);
      p.dy = -(9 * 4 * t * (800 - t)) / (800 * 800);
      p.armRightUp = (v.now / 200) % 2 == 0;
      if ((v.now / 400) % 2 == 0) {
        rect(x0 + 46, 3, 4, 10, kYellow);
        rect(x0 + 46, 15, 4, 4, kYellow);
      }
      break;
    }
    case State::Done: {
      p.eyes = Eyes::Happy;
      const bool party = v.since < 4000;
      p.armLeftUp = p.armRightUp = party;
      if (party) {
        p.dy = -static_cast<int>((v.now / 150) % 2) * 2;
        confetti(v.since);
      }
      break;
    }
  }

  mascot(x0, y0, p);
  sidebar(v);
}

}  // namespace hud
