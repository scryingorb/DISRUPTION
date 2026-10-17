#pragma once
#include "DisruptionLookAndFeel.h"
#include "VisualizerBridge.h"
namespace disruption::ui {
class DisruptionVisualizer final : public juce::Component {
public:
  explicit DisruptionVisualizer(VisualizerBridge &b) : bridge(b) {
    setInterceptsMouseClicks(false, false);
    bridge.setActive(true);
  }
  ~DisruptionVisualizer() override { bridge.setActive(false); }
  void update(float amount, int engine, bool bypass);
  void paint(juce::Graphics &) override;
  float currentPeak() const noexcept { return peak; }
  static constexpr int particleCount = 24;

private:
  struct Particle {
    float x = 0, y = 0, vx = 0, vy = 0, life = 0;
  };
  VisualizerBridge &bridge;
  std::array<float, 1024> incoming{};
  std::array<float, 256> wave{};
  std::array<Particle, particleCount> particles{};
  float damage = 0, peak = 0;
  int mode = 0;
  bool bypassed = false;
  uint32_t frame = 0;
};
} // namespace disruption::ui
