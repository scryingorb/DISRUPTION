#pragma once
#include "ModeEngine.h"
namespace disruption {
class BurnEngine final : public ModeEngine {
public:
 static float transfer(float sample, float damage) noexcept;
 float process(float sample, int, float damage) noexcept override { return transfer(sample, damage); }
};
}
