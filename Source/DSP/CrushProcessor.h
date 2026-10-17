#pragma once
#include "DigitalQuantizer.h"
namespace disruption {
class CrushProcessor {
  double rate = 44100, target = 44100, phase = 0;
  bool first = true, capture = false;
  std::array<float, 2> held{};
  float last = -1, amount = 0, lower = 32767.5f, upper = 32767.5f, fraction = 0;

public:
  static float effectiveBits(float percent) noexcept {
    return 16 - 11 * std::pow(unitDamage(percent), 0.85f);
  }
  static double requestedSampleRate(float percent) noexcept {
    return 96000 *
           std::exp(std::log(2200.0 / 96000) *
                    std::pow(static_cast<double>(unitDamage(percent)), 1.7));
  }
  void prepare(double sr) noexcept {
    rate = sr;
    reset();
  }
  void reset() noexcept {
    phase = 0;
    first = true;
    capture = false;
    held.fill(0);
    last = -1;
  }
  void beginFrame(float percent) noexcept {
    if (percent != last) {
      last = percent;
      amount = unitDamage(percent);
      target = std::min(rate, requestedSampleRate(percent));
      float bits = effectiveBits(percent);
      int n = static_cast<int>(std::floor(bits));
      lower = std::ldexp(1.0f, n - 1) - 0.5f;
      upper = n < 16 ? std::ldexp(1.0f, n) - 0.5f : lower;
      fraction = bits - n;
    }
    capture = first;
    if (first) {
      first = false;
      return;
    }
    phase += target / rate;
    if (phase >= 1 - 1e-12) {
      capture = true;
      phase = std::max(0.0, phase - 1);
    }
  }
  float process(float x, int ch, float) noexcept {
    if (capture)
      held[static_cast<size_t>(ch)] = x;
    if (amount == 0)
      return x;
    float q = 2 * quantizeLevels(held[static_cast<size_t>(ch)] * 0.5f, lower,
                                 upper, fraction);
    return x * (1 - amount) + q * amount;
  }
  double getEffectiveRate() const noexcept { return target; }
  bool didCaptureThisFrame() const noexcept { return capture; }
};
} // namespace disruption
