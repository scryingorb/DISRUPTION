#include "BurnEngine.h"
#include "TransferSafety.h"
namespace disruption {
float BurnEngine::transfer(float sample, float damage) noexcept {
 const float x = finiteInput(sample), d = unitDamage(damage);
 const float threshold = 1.4f - 0.65f * d;
 const float bias = 0.18f * d * d;
 const float centre = std::tanh(bias);
 // Subtract the zero-input bias and normalize the small-signal slope to unity.
 // DAMAGE changes headroom and even-harmonic character, rather than input gain.
 return threshold * (std::tanh(x / threshold + bias) - centre) / (1.0f - centre * centre);
}
}
