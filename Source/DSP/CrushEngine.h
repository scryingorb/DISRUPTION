#pragma once
#include "ModeEngine.h"
namespace disruption {
class CrushEngine final : public ModeEngine {
public:
 void prepare(double sampleRate) noexcept override;
 void reset() noexcept override;
 // Exactly once per processing frame, before either channel: one shared clock.
 void beginFrame(float damage) noexcept override;
 float process(float sample, int channel, float) noexcept override;
 static float effectiveBits(float damage) noexcept;
 static double requestedSampleRate(float damage) noexcept;
 static float quantize(float sample, float damage) noexcept;
 double getEffectiveRate() const noexcept { return targetRate; }
 bool didCaptureThisFrame() const noexcept { return capture; }
private:
 double rate=44100.0, targetRate=44100.0, phase=0.0;
 float lastDamage=-1.0f, lowerLevels=32768.0f, upperLevels=32768.0f, bitFraction=0.0f;
 bool firstFrame=true, capture=false;
 std::array<float,2> held {};
 static float quantizeWithLevels(float, float, float, float) noexcept;
};
}
