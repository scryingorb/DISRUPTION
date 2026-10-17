#pragma once
#include "DisruptionLookAndFeel.h"
namespace disruption::ui {
class ModeSelector final : public juce::Component {
public:
  explicit ModeSelector(juce::AudioProcessorValueTreeState &state) {
    const char *names[] = {"BURN", "BREAK", "RIP", "CRUSH", "CHAOS"};
    const char *help[] = {"Warm saturation", "Hard clipping",
                          "Asymmetric distortion", "Digital degradation",
                          "Folded destruction"};
    for (int i = 0; i < 5; ++i) {
      buttons[i].setButtonText(names[i]);
      buttons[i].setComponentID("mode-" + juce::String(names[i]));
      buttons[i].setTitle(names[i]);
      buttons[i].setTooltip(help[i]);
      buttons[i].getProperties().set("engine", i);
      addAndMakeVisible(buttons[i]);
      buttons[i].onClick = [this, i] {
        attachment->setValueAsCompleteGesture(static_cast<float>(i));
      };
    }
    attachment = std::make_unique<juce::ParameterAttachment>(
        *state.getParameter("mode"),
        [this](float value) {
          for (int i = 0; i < 5; ++i)
            buttons[i].setToggleState(i == static_cast<int>(value),
                                      juce::dontSendNotification);
        },
        nullptr);
    attachment->sendInitialUpdate();
  }
  void resized() override {
    int width = (getWidth() - 4 * 8) / 5;
    for (int i = 0; i < 5; ++i)
      buttons[i].setBounds(i * (width + 8), 0, width, getHeight());
  }

private:
  std::array<juce::TextButton, 5> buttons;
  std::unique_ptr<juce::ParameterAttachment> attachment;
};
} // namespace disruption::ui
