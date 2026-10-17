#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
namespace disruption {
// One audio producer and the processor's single editor consumer. No waits or
// allocations.
class VisualizerBridge {
  static_assert(std::atomic<bool>::is_always_lock_free &&
                    std::atomic<int>::is_always_lock_free,
                "The visualization bridge requires lock-free atomics.");

public:
  static constexpr int capacity = 1024;
  void setActive(bool value) noexcept {
    active.store(value, std::memory_order_release);
  }
  bool isActive() const noexcept {
    return active.load(std::memory_order_relaxed);
  }
  void push(const juce::AudioBuffer<float> &buffer) noexcept {
    if (!active.load(std::memory_order_relaxed))
      return;
    for (int n = 0; n < buffer.getNumSamples(); n += 32) {
      int a = 0, b = 0, c = 0, d = 0;
      fifo.prepareToWrite(1, a, b, c, d);
      if (b == 0)
        break;
      float sample = buffer.getSample(0, n);
      if (buffer.getNumChannels() > 1) {
        float right = buffer.getSample(1, n);
        if (std::abs(right) > std::abs(sample))
          sample = right;
      }
      samples[static_cast<size_t>(a)] = sample;
      fifo.finishedWrite(1);
    }
  }
  int drain(float *output, int limit) noexcept {
    int a = 0, b = 0, c = 0, d = 0;
    fifo.prepareToRead(limit, a, b, c, d);
    for (int i = 0; i < b; ++i)
      output[i] = samples[static_cast<size_t>(a + i)];
    for (int i = 0; i < d; ++i)
      output[b + i] = samples[static_cast<size_t>(c + i)];
    fifo.finishedRead(b + d);
    return b + d;
  }

private:
  std::atomic<bool> active{false};
  juce::AbstractFifo fifo{capacity};
  std::array<float, capacity> samples{};
};
} // namespace disruption
