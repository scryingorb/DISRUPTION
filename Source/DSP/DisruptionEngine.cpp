#include "DisruptionEngine.h"
#include <cmath>
namespace disruption {
float DisruptionEngine::safe(float x) noexcept {
  if (!std::isfinite(x))
    return 0;
  // Emergency protection only: far outside normal audio levels (+120 dBFS).
  return juce::jlimit(-1000000.0f, 1000000.0f, x);
}
void DisruptionEngine::prepare(double rate, const Controls &c,
                               bool oversampled) {
  rate = std::isfinite(rate) && rate > 0 ? rate : 44100.0;
  highQuality = oversampled;
  if (highQuality) {
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(2);
    oversampler->clearOversamplingStages();
    oversampler->addOversamplingStage(
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, .05f,
        -90.f, .06f, -75.f);
    oversampler->addOversamplingStage(
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, .10f,
        -80.f, .12f, -70.f);
    oversampler->setUsingIntegerLatency(true);
    oversampler->initProcessing(1);
    frameBuffer.setSize(2, 1);
    hqLatency =
        static_cast<int>(std::round(oversampler->getLatencyInSamples()));
  } else {
    oversampler.reset();
    hqLatency = 0;
  }
  for (size_t i = 0; i < modes.size(); ++i)
    modes[i]->prepare(highQuality && i != 3 ? rate * 4 : rate);
  tone.prepare(rate);
  body.prepare(rate);
  crush.prepare(rate);
  for (auto *s : {&drive, &output, &mix, &bypass, &dcBlend, &toneAmount,
                  &bodyAmount, &crushAmount})
    s->reset(rate, 0.02);
  drive.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(c.drive));
  output.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(c.output));
  mix.setCurrentAndTargetValue(c.mix * 0.01f);
  bypass.setCurrentAndTargetValue(c.bypass ? 1.0f : 0.0f);
  dcBlend.setCurrentAndTargetValue(c.mix > 0.0f ? 1.0f : 0.0f);
  toneAmount.setCurrentAndTargetValue(c.tone);
  bodyAmount.setCurrentAndTargetValue(c.body);
  crushAmount.setCurrentAndTargetValue(c.crush);
  damage.reset(rate, 0.01);
  damage.setCurrentAndTargetValue(c.damage);
  for (size_t mode = 0; mode < modeWeights.size(); ++mode) {
    modeWeights[mode].reset(rate, 0.005);
    modeWeights[mode].setCurrentAndTargetValue(
        static_cast<int>(mode) == juce::jlimit(0, 4, c.mode) ? 1.0f : 0.0f);
  }
  dcPole = static_cast<float>(
      std::exp(-juce::MathConstants<double>::twoPi * 5.0 / rate));
  reset();
}
void DisruptionEngine::reset() noexcept {
  previousInput.fill(0);
  previousOutput.fill(0);
  delayPosition = 0;
  for (auto &a : dryDelay)
    a.fill(0);
  for (auto &a : crushDelay)
    a.fill(0);
  if (oversampler)
    oversampler->reset();
  for (auto *e : modes)
    e->reset();
  tone.reset();
  body.reset();
  crush.reset();
}
void DisruptionEngine::process(juce::AudioBuffer<float> &buffer,
                               const Controls &c, bool hostBypass) noexcept {
  juce::ScopedNoDenormals guard;
  if (buffer.getNumChannels() < 1 || buffer.getNumChannels() > 2) {
    buffer.clear();
    return;
  }
  drive.setTargetValue(juce::Decibels::decibelsToGain(c.drive));
  output.setTargetValue(juce::Decibels::decibelsToGain(c.output));
  mix.setTargetValue(c.mix * 0.01f);
  bypass.setTargetValue(c.bypass || hostBypass ? 1.0f : 0.0f);
  dcBlend.setTargetValue(c.mix > 0.0f ? 1.0f : 0.0f);
  damage.setTargetValue(c.damage);
  toneAmount.setTargetValue(c.tone);
  bodyAmount.setTargetValue(c.body);
  crushAmount.setTargetValue(c.crush);
  const int selected = juce::jlimit(0, 4, c.mode);
  for (size_t mode = 0; mode < modeWeights.size(); ++mode)
    modeWeights[mode].setTargetValue(static_cast<int>(mode) == selected ? 1.0f
                                                                        : 0.0f);
  for (int i = 0; i < buffer.getNumSamples(); ++i) {
    const float d = drive.getNextValue(), o = output.getNextValue(),
                m = mix.getNextValue(), b = bypass.getNextValue();
    const float damageValue = damage.getNextValue(),
                dcAmount = dcBlend.getNextValue();
    for (auto *mode : modes)
      mode->beginFrame(damageValue);
    tone.beginFrame(toneAmount.getNextValue());
    body.beginFrame(bodyAmount.getNextValue());
    crush.beginFrame(crushAmount.getNextValue());
    std::array<float, 5> weights{};
    for (size_t mode = 0; mode < weights.size(); ++mode)
      weights[mode] = modeWeights[mode].getNextValue();
    std::array<float, 2> highWet{}, delayedDry{}, delayedCrush{};
    if (highQuality) {
      for (int ch = 0; ch < 2; ++ch) {
        float raw =
            ch < buffer.getNumChannels() ? safe(buffer.getSample(ch, i)) : 0;
        float driven = safe(raw * d);
        frameBuffer.setSample(ch, 0, driven);
        delayedDry[ch] = dryDelay[ch][delayPosition];
        dryDelay[ch][delayPosition] = raw;
        delayedCrush[ch] = crushDelay[ch][delayPosition];
        crushDelay[ch][delayPosition] =
            weights[3] * crushing.process(driven, ch, damageValue);
      }
      juce::dsp::AudioBlock<float> block(frameBuffer);
      auto up = oversampler->processSamplesUp(block);
      for (size_t sub = 0; sub < up.getNumSamples(); ++sub) {
        for (size_t mode = 0; mode < modes.size(); ++mode)
          if (mode != 3)
            modes[mode]->beginFrame(damageValue);
        for (int ch = 0; ch < 2; ++ch) {
          float x = up.getSample(ch, sub), y = 0;
          for (size_t mode = 0; mode < modes.size(); ++mode)
            if (mode != 3 && (weights[mode] > 0 || mode >= 2))
              y += weights[mode] * modes[mode]->process(x, ch, damageValue);
          up.setSample(ch, sub, y);
        }
      }
      oversampler->processSamplesDown(block);
      for (int ch = 0; ch < 2; ++ch)
        highWet[ch] = frameBuffer.getSample(ch, 0) + delayedCrush[ch];
      delayPosition = (delayPosition + 1) % hqLatency;
    }
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
      const float dry =
          highQuality ? delayedDry[ch] : safe(buffer.getSample(ch, i));
      float wet = safe(dry * d);
      // HQ uses 4x nonlinear processing; intentional digital stages stay
      // native.
      const float driven = wet;
      wet = 0.0f;
      for (size_t mode = 0; !highQuality && mode < modes.size(); ++mode) {
        // Keep RIP/CHAOS DC histories and the stereo CRUSH clock/input warm.
        if (weights[mode] > 0.0f || mode >= 2)
          wet += weights[mode] * modes[mode]->process(driven, ch, damageValue);
      }
      if (highQuality)
        wet = highWet[ch];
      wet = crush.process(wet, ch, c.crush);
      wet = tone.process(wet, ch, c.tone);
      wet = body.process(wet, ch, c.body);
      // Convex interpolation avoids cancellation of small wet audio against
      // extremely overloaded dry input when MIX is fully wet.
      float processed = safe((dry * (1.0f - m) + wet * m) * o);
      const auto index = static_cast<size_t>(ch);
      float dc = safe(processed - previousInput[index] +
                      dcPole * previousOutput[index]);
      previousInput[index] = processed;
      previousOutput[index] = std::abs(dc) < 1.0e-20f ? 0.0f : dc;
      // Fade the final DC correction at the fully-dry endpoint. The old m>0
      // gate jumped there. Settled nonzero MIX retains full DC protection;
      // settled MIX=0 is transparent. RIP also blocks DC before mixing.
      processed += (dc - processed) * dcAmount;
      buffer.setSample(ch, i, safe(processed * (1.0f - b) + dry * b));
    }
  }
}
} // namespace disruption
