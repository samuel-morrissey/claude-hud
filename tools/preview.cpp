// Gera uma folha com quadros de cada estado, em PPM, na saída padrão.
// Uma linha por estado e origem; as colunas são instantes sucessivos.

#include <cstdio>
#include <vector>

#include "../src/render.h"

int main() {
  using namespace hud;

  struct Row {
    State state;
    Source source;
    uint8_t sessions;
  };
  const Row rows[] = {
      {State::Sleep, Source::Pc, 0},     {State::Idle, Source::Pc, 1},     {State::Thinking, Source::Pc, 1},
      {State::Working, Source::Cloud, 2}, {State::Waiting, Source::Phone, 1}, {State::Done, Source::Cloud, 12},
  };
  const int kRows = sizeof(rows) / sizeof(rows[0]);
  const int kCols = 6;
  const int kScale = 3;
  const int kGap = 4;
  const uint32_t kStepMs = 130;

  const int sheet_w = kCols * (W * kScale + kGap) + kGap;
  const int sheet_h = kRows * (H * kScale + kGap) + kGap;
  std::vector<uint8_t> sheet(sheet_w * sheet_h * 3, 60);
  std::vector<uint16_t> fb(W * H);

  for (int r = 0; r < kRows; r++) {
    for (int c = 0; c < kCols; c++) {
      const uint32_t t = 1000 + c * kStepMs;
      const View view{rows[r].state, rows[r].source, rows[r].sessions, t, c * kStepMs, r == 1};
      render(view, fb.data());
      const int ox = kGap + c * (W * kScale + kGap);
      const int oy = kGap + r * (H * kScale + kGap);
      for (int y = 0; y < H * kScale; y++) {
        for (int x = 0; x < W * kScale; x++) {
          const uint16_t p = fb[(y / kScale) * W + x / kScale];
          uint8_t* out = &sheet[((oy + y) * sheet_w + ox + x) * 3];
          out[0] = (p >> 11) << 3;
          out[1] = ((p >> 5) & 0x3F) << 2;
          out[2] = (p & 0x1F) << 3;
        }
      }
    }
  }

  std::printf("P6\n%d %d\n255\n", sheet_w, sheet_h);
  std::fwrite(sheet.data(), 1, sheet.size(), stdout);
  return 0;
}
