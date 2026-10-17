#pragma once
#include "TransferSafety.h"
#include <algorithm>
#include <array>
namespace disruption {
inline float quantizeLevels(float sample, float lower, float upper,
                            float fraction) noexcept {
  const float x = std::max(-1.0f, std::min(1.0f, finiteInput(sample)));
  const float low = std::round(x * lower) / lower;
  if (fraction == 0)
    return low;
  const float high = std::round(x * upper) / upper;
  return low * (1 - fraction) + high * fraction;
}
} // namespace disruption
