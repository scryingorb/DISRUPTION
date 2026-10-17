#pragma once
#include "BodyProcessor.h"
#include "BreakEngine.h"
#include "BurnEngine.h"
#include "ChaosEngine.h"
#include "CrushEngine.h"
#include "CrushProcessor.h"
#include "ModeEngine.h"
#include "RipEngine.h"
#include "ToneProcessor.h"
#include <juce_dsp/juce_dsp.h>
namespace disruption {
class DisruptionEngine {
public:
  void prepare(double sampleRate, const Controls &, bool oversampled = false);
  void reset() noexcept;
  void process(juce::AudioBuffer<float> &, const Controls &,
               bool hostBypass = false) noexcept;
  int latency() const noexcept { return hqLatency; }

private:
  bool highQuality = false;
  int hqLatency = 0, delayPosition = 0;
  std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
  juce::AudioBuffer<float> frameBuffer;
  std::array<std::array<float, 256>, 2> dryDelay{}, crushDelay{};
  BurnEngine burn;
  BreakEngine breaking;
  RipEngine rip;
  CrushEngine crushing;
  ChaosEngine chaos;
  std::array<ModeEngine *, 5> modes{
      {&burn, &breaking, &rip, &crushing, &chaos}};
  ToneProcessor tone;
  BodyProcessor body;
  CrushProcessor crush;
  juce::SmoothedValue<float> drive, output, mix, bypass, damage, dcBlend,
      toneAmount, bodyAmount, crushAmount;
  std::array<juce::SmoothedValue<float>, 5> modeWeights;
  std::array<float, 2> previousInput{}, previousOutput{};
  float dcPole = 0.999f;
  static float safe(float) noexcept;
};
} // namespace disruption
