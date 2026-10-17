#include "PluginEditor.h"
#include <BinaryData.h>
DisruptionEditor::DisruptionEditor(DisruptionProcessor &p)
    : AudioProcessorEditor(p), processor(p), visualizer(p.visualizerBridge()),
      modes(p.parameters), presets(p) {
  setLookAndFeel(&theme);
  setOpaque(true);
  setResizable(false, false);
  // Retain only appropriately sized 2x artwork; the full decode is temporary.
  auto original = juce::ImageFileFormat::loadFrom(
      BinaryData::DisruptionArtwork_png, BinaryData::DisruptionArtwork_pngSize);
  wordmark = original.getClippedImage({55, 822, 1136, 214})
                 .rescaled(640, 121, juce::Graphics::highResamplingQuality);
  emblem = original.getClippedImage({315, 175, 600, 580})
               .rescaled(128, 124, juce::Graphics::highResamplingQuality);
  addAndMakeVisible(visualizer);
  addAndMakeVisible(modes);
  addAndMakeVisible(presets);
  const char *ids[] = {"drive", "damage", "tone",  "body",
                       "crush", "mix",    "output"};
  const char *titles[] = {"DRIVE", "DAMAGE", "TONE",  "BODY",
                          "CRUSH", "MIX",    "OUTPUT"};
  const char *tips[] = {
      "Controls how hard the signal hits the distortion engine.",
      "Controls how destructive the selected engine becomes.",
      "Shifts the processed signal darker or brighter.",
      "Restores thickness and low / low-mid weight.",
      "Adds additional digital degradation.",
      "Blends clean and processed audio.",
      "Controls final output level."};
  for (size_t i = 0; i < knobs.size(); ++i) {
    knobs[i] = std::make_unique<disruption::ui::DisruptionKnob>(
        p.parameters, ids[i], titles[i], tips[i], i == 1);
    knobs[i]->setBufferedToImage(true);
    addAndMakeVisible(*knobs[i]);
  }
  hq.setComponentID("hq");
  bypass.setComponentID("bypass");
  hq.setTooltip("Higher-quality 4x oversampled nonlinear processing. Digital "
                "CRUSH remains native; latency is unchanged.");
  bypass.setTooltip("Bypass the effect while preserving project timing.");
  addAndMakeVisible(hq);
  addAndMakeVisible(bypass);
  hqAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          p.parameters, "hq", hq);
  bypassAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          p.parameters, "bypass", bypass);
  setSize(editorWidth, editorHeight);
  refreshVisuals();
  startTimerHz(30);
}
DisruptionEditor::~DisruptionEditor() {
  stopTimer();
  setLookAndFeel(nullptr);
}
void DisruptionEditor::refreshVisuals() {
  damage = processor.parameters.getRawParameterValue("damage")->load();
  mode = static_cast<int>(
      processor.parameters.getRawParameterValue("mode")->load());
  bypassed = processor.parameters.getRawParameterValue("bypass")->load() >= .5f;
  visualizer.update(damage, mode, bypassed);
  presets.sync();
  repaint(24, 558, 852, 30);
}
void DisruptionEditor::paint(juce::Graphics &g) {
  using namespace disruption::ui;
  auto r = getLocalBounds().toFloat();
  g.setGradientFill(juce::ColourGradient(
      juce::Colour(0xff292d30), 0, 0, juce::Colour(0xff0c0e10), 0,
      static_cast<float>(getHeight()), false));
  g.fillRect(r);
  g.setColour(juce::Colour(0xff080a0b));
  g.fillRoundedRectangle(r.reduced(9), 9);
  g.setColour(juce::Colour(0xff50575c));
  g.drawRoundedRectangle(r.reduced(9), 9, 1);
  g.setColour(juce::Colour(0xff22272b));
  g.drawRoundedRectangle(r.reduced(12), 7, 1);
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff272c30), 0, 16,
                                         juce::Colour(0xff14181b), 0, 104,
                                         false));
  g.fillRoundedRectangle(juce::Rectangle<float>(18, 16, 864, 88), 4);
  g.drawImage(emblem, juce::Rectangle<float>(28, 25, 58, 56),
              juce::RectanglePlacement::centred);
  g.drawImage(wordmark, juce::Rectangle<float>(94, 21, 302, 57),
              juce::RectanglePlacement::centred);
  g.setColour(juce::Colour(0xffa0a6aa));
  g.setFont(
      juce::Font(juce::FontOptions("Bahnschrift", 12, juce::Font::plain)));
  g.drawText("V O X X L A B", 99, 78, 170, 18,
             juce::Justification::centredLeft);
  g.setFont(11);
  g.drawText(DISRUPTION_DISPLAY_VERSION, 326, 79, 68, 16, juce::Justification::centredRight);
  g.setColour(orange().withAlpha(bypassed ? .25f : .65f));
  g.drawLine(24, 109, 876, 109, 1);
  g.drawLine(25, 166, 875, 166, .6f);
  // Restrained engraved geometry and segmented rails; no full-panel noisy
  // texture.
  g.setColour(juce::Colour(0xff353b40));
  for (int side : {0, 1}) {
    float x = side ? 856.f : 43.f;
    g.drawLine(x, 185, x, 548, 1);
    for (int i = 0; i < 8; ++i)
      g.drawLine(x - 4, 192 + i * 49.f, x + 4, 192 + i * 49.f, 1);
  }
  g.setColour(juce::Colour(0xff51585d).withAlpha(.3f));
  juce::Path seam;
  seam.startNewSubPath(268, 188);
  seam.lineTo(295, 188);
  seam.lineTo(306, 178);
  seam.lineTo(593, 178);
  seam.lineTo(605, 188);
  seam.lineTo(632, 188);
  g.strokePath(seam, juce::PathStrokeType(1));
  for (auto point : {juce::Point<int>(22, 22), juce::Point<int>(878, 22),
                     juce::Point<int>(22, 578), juce::Point<int>(878, 578)}) {
    g.setColour(juce::Colour(0xff434b50));
    g.fillEllipse(point.x - 3.f, point.y - 3.f, 6, 6);
    g.setColour(juce::Colour(0xff111416));
    g.drawLine(point.x - 2.f, point.y + 1.f, point.x + 2.f, point.y - 1.f, 1);
  }
  const char *personalities[] = {"THERMAL SATURATION", "HARD CLIP",
                                 "ASYMMETRIC TEAR", "DIGITAL DEGRADATION",
                                 "WAVEFOLD DESTRUCTION"};
  g.setColour(juce::Colour(0xffa1a7ac));
  g.setFont(
      juce::Font(juce::FontOptions("Bahnschrift", 11, juce::Font::plain)));
  g.drawText("DESTRUCTION AMOUNT", 318, 516, 264, 18,
             juce::Justification::centred);
  g.setColour(juce::Colour(0xff30363b));
  g.drawLine(28, 558, 872, 558, 1);
  g.setColour(orange().withAlpha(bypassed ? .3f : .9f));
  g.fillEllipse(33, 572, 5, 5);
  g.setColour(juce::Colour(0xffa4aaae));
  g.drawText(bypassed                           ? "SIGNAL BYPASSED"
             : visualizer.currentPeak() > .001f ? "SIGNAL ACTIVE"
                                                : "SIGNAL IDLE",
             47, 566, 178, 18, juce::Justification::centredLeft);
  g.drawText(personalities[juce::jlimit(0, 4, mode)], 245, 566, 410, 18,
             juce::Justification::centred);
  g.drawText("VOXXLAB / DISRUPTION", 663, 566, 204, 18,
             juce::Justification::centredRight);
}
void DisruptionEditor::resized() {
  presets.setBounds(416, 24, 345, 70);
  hq.setBounds(781, 24, 96, 28);
  bypass.setBounds(781, 64, 96, 28);
  modes.setBounds(26, 119, 848, 39);
  visualizer.setComponentID("visualizer");
  visualizer.setBounds(27, 178, 846, 370);
  const int x[] = {92, 309, 653, 92, 653, 92, 653};
  const int y[] = {184, 204, 184, 311, 311, 438, 438};
  for (size_t i = 0; i < knobs.size(); ++i)
    knobs[i]->setBounds(x[i], y[i], i == 1 ? 282 : 155, i == 1 ? 308 : 115);
}
