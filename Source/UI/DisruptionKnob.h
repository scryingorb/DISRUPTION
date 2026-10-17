#pragma once
#include "DisruptionLookAndFeel.h"
namespace disruption::ui {
class DisruptionKnob final : public juce::Component {
  class Rotary final : public juce::Slider {
  public:
    void mouseDown(const juce::MouseEvent &e) override {
      setMouseDragSensitivity(e.mods.isShiftDown() ? 900 : 180);
      juce::Slider::mouseDown(e);
    }
  };

public:
  DisruptionKnob(juce::AudioProcessorValueTreeState &state,
                 const juce::String &id, const juce::String &title,
                 const juce::String &help, bool signature = false)
      : large(signature) {
    setComponentID("group-" + id);
    label.setText(title, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(juce::FontOptions(
        "Bahnschrift", large ? 21.f : 14.f, juce::Font::bold)));
    addAndMakeVisible(label);
    slider.setComponentID(id);
    slider.setName(title);
    slider.setTooltip(help +
                      " Double-click resets; Shift-drag adjusts finely.");
    slider.setTitle(title);
    slider.setDescription(help);
    slider.getProperties().set("signature", signature);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, large ? 116 : 106,
                           large ? 30 : 24);
    bool db = id == "drive" || id == "output";
    addAndMakeVisible(slider);
    attachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            state, id, slider);
    slider.textFromValueFunction = [db](double v) {
      return (db && v > 0 ? juce::String("+") : juce::String()) +
             juce::String(v, db ? 1 : 0) + (db ? " dB" : "%");
    };
    slider.valueFromTextFunction = [](const juce::String &s) {
      return s.getDoubleValue();
    };
    slider.updateText();
    auto *parameter = state.getParameter(id);
    slider.setDoubleClickReturnValue(
        true, parameter->convertFrom0to1(parameter->getDefaultValue()));
  }
  void resized() override {
    label.setBounds(0, 0, getWidth(), large ? 30 : 21);
    slider.setBounds(0, large ? 30 : 21, getWidth(),
                     getHeight() - (large ? 30 : 21));
  }
  juce::Slider &control() { return slider; }

private:
  bool large;
  juce::Label label;
  Rotary slider;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      attachment;
};
} // namespace disruption::ui
