#pragma once
#include <algorithm>
#include <cmath>
namespace disruption {
// Shared validation also protects individual engine calls in developer tools.
inline float finiteInput(float x) noexcept {
 return std::isfinite(x) ? std::max(-1000000.0f, std::min(1000000.0f, x)) : 0.0f;
}
inline float unitDamage(float percent) noexcept {
 return std::isfinite(percent) ? std::max(0.0f, std::min(1.0f, percent * 0.01f)) : 0.0f;
}
}
