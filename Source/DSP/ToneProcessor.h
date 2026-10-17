#pragma once
#include "ParallelFilter.h"
#include <algorithm>
namespace disruption {
class ToneProcessor {
  ParallelFilter split;
  float last = 999, lowGain = 1, highGain = 1;

public:
  void prepare(double rate) noexcept {
    split.prepare(rate, 1200);
    last = 999;
  }
  void reset() noexcept { split.reset(); }
  void beginFrame(float percent) noexcept {
    if (percent == last)
      return;
    last = percent;
    lowGain = std::pow(10.0f, -percent * 0.003f);
    highGain = std::pow(10.0f, percent * 0.003f);
  }
  float process(float x, int ch, float) noexcept {
    float low = split.low(x, ch);
    return last == 0 ? x : x + (lowGain - 1) * low + (highGain - 1) * (x - low);
  }
};
} // namespace disruption
