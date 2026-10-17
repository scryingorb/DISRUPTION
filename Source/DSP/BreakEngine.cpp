#include "BreakEngine.h"
#include "TransferSafety.h"
namespace disruption {
float BreakEngine::transfer(float sample, float damage) noexcept {
 const float x = finiteInput(sample), d = unitDamage(damage);
 const float threshold = 1.1f - 0.42f * d;
 const float width = threshold * (0.8f * (1.0f - d) * (1.0f - d) + 0.01f);
 const float start = threshold - width, end = threshold + width;
 const float magnitude = std::abs(x);
 float clipped = magnitude;
 if (magnitude >= end) clipped = threshold;
 else if (magnitude > start) {
  const float u = magnitude - start;
  clipped = start + u - u * u / (4.0f * width);
 }
 // The quadratic knee joins slope 1 to slope 0 continuously. Even maximum
 // DAMAGE retains a narrow knee; there is no abrupt algorithm switch.
 return std::copysign(clipped, x);
}
}
