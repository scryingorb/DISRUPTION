#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
namespace disruption::ui {
inline juce::Colour orange() { return juce::Colour(0xffff651f); }
inline juce::Colour steel() { return juce::Colour(0xffd5d7d8); }
class DisruptionLookAndFeel final : public juce::LookAndFeel_V4 {
public:
  DisruptionLookAndFeel();
  void drawRotarySlider(juce::Graphics &, int, int, int, int, float, float,
                        float, juce::Slider &) override;
  void drawButtonBackground(juce::Graphics &, juce::Button &,
                            const juce::Colour &, bool, bool) override;
  void drawButtonText(juce::Graphics &, juce::TextButton &, bool,
                      bool) override;
  void drawToggleButton(juce::Graphics &, juce::ToggleButton &, bool,
                        bool) override;
  void drawComboBox(juce::Graphics &, int, int, bool, int, int, int, int,
                    juce::ComboBox &) override;
  juce::Font getComboBoxFont(juce::ComboBox &) override;
  juce::Font getLabelFont(juce::Label &) override;
};
} // namespace disruption::ui
