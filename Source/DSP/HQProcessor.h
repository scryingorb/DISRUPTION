#pragma once
#include "DisruptionEngine.h"
namespace disruption {
class HQProcessor {
  DisruptionEngine native, high;
  juce::AudioBuffer<float> nativeBuffer, highBuffer;
  juce::SmoothedValue<float> fade;
  std::array<std::array<float, 256>, 2> nativeDelay{};
  int latencySamples = 0, position = 0;
  bool highRunning = false;

public:
  void prepare(double rate, const Controls &c) {
    rate = std::isfinite(rate) && rate > 0 ? rate : 44100.0;
    native.prepare(rate, c);
    high.prepare(rate, c, true);
    latencySamples = high.latency();
    nativeBuffer.setSize(2, 1024);
    highBuffer.setSize(2, 1024);
    fade.reset(rate, .02);
    fade.setCurrentAndTargetValue(c.hq && c.mode != 3 ? 1.f : 0.f);
    highRunning = c.hq && c.mode != 3;
    reset();
  }
  int latency() const noexcept { return latencySamples; }
  void reset() noexcept {
    native.reset();
    high.reset();
    position = 0;
    for (auto &a : nativeDelay)
      a.fill(0);
  }
  void process(juce::AudioBuffer<float> &buffer, const Controls &c,
               bool bypass = false) noexcept {
    if (buffer.getNumChannels() < 1 || buffer.getNumChannels() > 2) {
      buffer.clear();
      return;
    }
    fade.setTargetValue(c.hq && c.mode != 3 ? 1.f : 0.f);
    bool needHigh = (c.hq && c.mode != 3) || fade.getCurrentValue() > 0;
    if (needHigh && !highRunning)
      high.reset();
    highRunning = needHigh;
    for (int offset = 0; offset < buffer.getNumSamples(); offset += 1024) {
      int n = std::min(1024, buffer.getNumSamples() - offset),
          channels = buffer.getNumChannels();
      float *np[2] = {nativeBuffer.getWritePointer(0),
                      nativeBuffer.getWritePointer(1)};
      float *hp[2] = {highBuffer.getWritePointer(0),
                      highBuffer.getWritePointer(1)};
      juce::AudioBuffer<float> a(np, channels, n), b(hp, channels, n);
      for (int ch = 0; ch < channels; ++ch) {
        a.copyFrom(ch, 0, buffer, ch, offset, n);
        if (needHigh)
          b.copyFrom(ch, 0, buffer, ch, offset, n);
      }
      native.process(a, c, bypass);
      if (needHigh)
        high.process(b, c, bypass);
      for (int i = 0; i < n; ++i) {
        float f = fade.getNextValue();
        for (int ch = 0; ch < channels; ++ch) {
          float delayed = nativeDelay[ch][position];
          nativeDelay[ch][position] = a.getSample(ch, i);
          float y =
              needHigh ? delayed * (1 - f) + b.getSample(ch, i) * f : delayed;
          buffer.setSample(ch, offset + i, y);
        }
        position = (position + 1) % latencySamples;
      }
    }
  }
};
} // namespace disruption
