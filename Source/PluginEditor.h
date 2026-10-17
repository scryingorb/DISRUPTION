#pragma once
#include "PluginProcessor.h"
#include "UI/DisruptionKnob.h"
#include "UI/DisruptionVisualizer.h"
#include "UI/ModeSelector.h"
#include "UI/PresetBar.h"
class DisruptionEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer {
public:
  explicit DisruptionEditor(DisruptionProcessor &);
  ~DisruptionEditor() override;
  void paint(juce::Graphics &) override;
  void resized() override;
  void refreshVisuals(); // Same message-thread update used by timer and preview
                         // validation.
  uint64_t animationFrameCount() const noexcept { return timerFrames; }
  static constexpr int editorWidth = 900, editorHeight = 600;

private:
  void timerCallback() override {
    ++timerFrames;
    refreshVisuals();
  }
  DisruptionProcessor &processor;
  disruption::ui::DisruptionLookAndFeel theme;
  juce::Image wordmark, emblem;
  disruption::ui::DisruptionVisualizer visualizer;
  disruption::ui::ModeSelector modes;
  disruption::ui::PresetBar presets;
  std::array<std::unique_ptr<disruption::ui::DisruptionKnob>, 7> knobs;
  juce::ToggleButton hq{"HQ"}, bypass{"BYPASS"};
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      hqAttachment, bypassAttachment;
  juce::TooltipWindow tooltips{this, 650};
  uint64_t timerFrames = 0;
  float damage = 0;
  int mode = 0;
  bool bypassed = false;
};
