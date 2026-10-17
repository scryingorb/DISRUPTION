#pragma once
#include "ModeEngine.h"
namespace disruption {
class RipEngine final : public ModeEngine {
public:
 static float transfer(float sample, float damage) noexcept;
 void prepare(double sampleRate) noexcept override;
 void reset() noexcept override;
 float process(float sample, int channel, float damage) noexcept override;
private:
 std::array<double, 2> previousInput {}, previousOutput {};
 double pole = 0.999, gain = 0.9995;
};
}
