#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
namespace disruption {
struct Controls { int mode=0; float drive=0, damage=0, tone=0, body=0, crush=0, mix=100, output=0; bool hq=false, bypass=false; };
class ModeEngine {
public:
 virtual ~ModeEngine() = default;
 virtual void prepare(double) noexcept {}
 virtual void reset() noexcept {}
 // One call per frame before channel processing; shared clocks/coefficients.
 virtual void beginFrame(float) noexcept {}
 virtual float process(float sample, int, float) noexcept { return sample; }
};
}

