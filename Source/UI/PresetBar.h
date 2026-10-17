#pragma once
#include "../PluginProcessor.h"
#include "DisruptionLookAndFeel.h"
namespace disruption::ui {
class PresetBar final : public juce::Component, private juce::Timer {
public:
  explicit PresetBar(DisruptionProcessor &);
  ~PresetBar() override { stopTimer(); }
  void resized() override;
  void sync();
  juce::String displayedName() const { return presetBox.getText(); }

private:
  void timerCallback() override { sync(); }
  void refreshPresets();
  void showResult(juce::Result);
  void savePreset();
  DisruptionProcessor &processor;
  juce::ComboBox presetBox;
  juce::Label presetStatus;
  juce::TextButton previous{"<"}, next{">"}, save{"SAVE"}, remove{"DEL"};
  std::vector<disruption::Preset> presetEntries;
};
} // namespace disruption::ui
