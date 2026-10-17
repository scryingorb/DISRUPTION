#pragma once
#include "ParallelFilter.h"
namespace disruption {
class BodyProcessor {
  ParallelFilter sub, top;
  float amount = 0;

public:
  void prepare(double rate) noexcept {
    sub.prepare(rate, 20);
    top.prepare(rate, 350);
  }
  void reset() noexcept {
    sub.reset();
    top.reset();
  }
  void beginFrame(float percent) noexcept { amount = percent * 0.01f; }
  float process(float x, int ch, float) noexcept {
    float band = top.low(x - sub.low(x, ch), ch);
    return amount == 0 ? x : x + amount * 1.5f * std::tanh(band / 1.5f);
  }
};
} // namespace disruption
