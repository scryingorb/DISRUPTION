#pragma once
#include <array>
#include <cmath>
namespace disruption {
// Bilinear one-pole low-pass. Fixed poles; independent stereo histories.
class ParallelFilter {
  double a = 0;
  std::array<double, 2> z{};

public:
  void prepare(double rate, double hz) noexcept {
    double g = std::tan(3.14159265358979323846 * hz / rate);
    a = g / (1 + g);
    reset();
  }
  void reset() noexcept { z.fill(0); }
  float low(float x, int ch) noexcept {
    auto &s = z[static_cast<size_t>(ch)];
    double v = (x - s) * a;
    double y = v + s;
    s = y + v;
    if (std::abs(s) < 1e-24)
      s = 0;
    return static_cast<float>(y);
  }
};
} // namespace disruption
