#include "RipEngine.h"
#include "TransferSafety.h"
namespace disruption {
float RipEngine::transfer(float sample, float damage) noexcept {
 const float x = finiteInput(sample), d = unitDamage(damage);
 const float positive = 1.3f - 0.95f * d;
 const float negative = 1.25f - 0.25f * d;
 constexpr float halfPi = 1.57079632679489661923f;
 // Unity-slope halves meet smoothly at zero. The positive half saturates
 // rapidly; the negative half has a long arctangent shoulder. No randomness.
 const float shaped = x >= 0.0f ? positive * std::tanh(x / positive)
                               : negative / halfPi * std::atan(halfPi * x / negative);
 return 1.12f * shaped;
}
void RipEngine::prepare(double rate) noexcept {
 if (!std::isfinite(rate) || rate <= 0.0) rate = 44100.0;
 pole = std::exp(-juce::MathConstants<double>::twoPi * 5.0 / rate);
 gain = (1.0 + pole) * 0.5; // Unity gain at Nyquist; 5 Hz high-pass.
 reset();
}
void RipEngine::reset() noexcept { previousInput.fill(0.0); previousOutput.fill(0.0); }
float RipEngine::process(float sample, int channel, float damage) noexcept {
 if (channel < 0 || channel >= 2) return 0.0f;
 const auto index = static_cast<size_t>(channel);
 const double input = transfer(sample, damage);
 const double y = gain * (input - previousInput[index]) + pole * previousOutput[index];
 previousInput[index] = input;
 previousOutput[index] = std::abs(y) < 1.0e-24 ? 0.0 : y;
 return static_cast<float>(previousOutput[index]);
}
}
