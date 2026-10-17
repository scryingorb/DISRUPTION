#include "DisruptionLookAndFeel.h"
namespace disruption::ui {
DisruptionLookAndFeel::DisruptionLookAndFeel() {
  setColour(juce::Slider::textBoxTextColourId, steel());
  setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff101215));
  setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff45484b));
  setColour(juce::Slider::textBoxHighlightColourId, orange().withAlpha(.4f));
  setColour(juce::Label::textColourId, steel());
  setColour(juce::ComboBox::textColourId, steel());
  setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff131518));
  setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff171a1d));
  setColour(juce::PopupMenu::highlightedBackgroundColourId,
            juce::Colour(0xff5e291b));
  setColour(juce::PopupMenu::textColourId, steel());
  setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff111316));
  setColour(juce::TextEditor::textColourId, steel());
}
juce::Font DisruptionLookAndFeel::getComboBoxFont(juce::ComboBox &) {
  return juce::Font(juce::FontOptions("Bahnschrift", 15, juce::Font::plain));
}
juce::Font DisruptionLookAndFeel::getLabelFont(juce::Label &l) {
  return l.getFont();
}
void DisruptionLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y,
                                             int w, int h, float position,
                                             float start, float end,
                                             juce::Slider &slider) {
  bool large = slider.getProperties().getWithDefault("signature", false);
  float radius = juce::jmin(w, h) * .5f - 9;
  float cx = x + w * .5f, cy = y + h * .5f;
  auto rect =
      juce::Rectangle<float>(cx - radius, cy - radius, radius * 2, radius * 2);
  float angle = start + position * (end - start);
  auto hot =
      orange().interpolatedWith(juce::Colour(0xffff321c), position * .65f);
  float ring = large ? 7.f : 3.5f;
  g.setColour(juce::Colours::black.withAlpha(.8f));
  g.fillEllipse(rect.translated(0, 5).expanded(3));
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff656b70), cx - radius,
                                         cy - radius, juce::Colour(0xff15181b),
                                         cx + radius, cy + radius, false));
  g.fillEllipse(rect);
  g.setColour(juce::Colour(0xff70767b).withAlpha(.4f));
  g.drawEllipse(rect, 1);
  auto arcRect = rect.reduced(large ? 13.f : 6.f);
  juce::Path track;
  track.addCentredArc(cx, cy, arcRect.getWidth() * .5f,
                      arcRect.getHeight() * .5f, 0, start, end, true);
  g.setColour(juce::Colour(0xff0a0b0c));
  g.strokePath(track, juce::PathStrokeType(ring + 4));
  juce::Path arc;
  arc.addCentredArc(cx, cy, arcRect.getWidth() * .5f, arcRect.getHeight() * .5f,
                    0, start, angle, true);
  for (int i = 3; i > 0; --i) {
    g.setColour(hot.withAlpha((large ? .05f : .025f) * (1 + position)));
    g.strokePath(arc, juce::PathStrokeType(ring + i * 3.f));
  }
  g.setColour(hot.withAlpha(.35f + .65f * position));
  g.strokePath(arc, juce::PathStrokeType(ring, juce::PathStrokeType::curved,
                                         juce::PathStrokeType::rounded));
  auto face = rect.reduced(large ? 24.f : 14.f);
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff353b40), cx,
                                         face.getY(), juce::Colour(0xff101316),
                                         cx, face.getBottom(), false));
  g.fillEllipse(face);
  g.setColour(juce::Colour(0xff4a5156));
  g.drawEllipse(face, 1);
  for (int i = 0; i < (large ? 37 : 17); ++i) {
    float a = start + i * (end - start) / (large ? 36.f : 16.f);
    float inner = radius - (large ? 3 : 1), outer = radius + (large ? 4 : 3);
    g.setColour(i <= position * (large ? 36 : 16) ? hot.withAlpha(.65f)
                                                  : juce::Colour(0xff51565a));
    g.drawLine(cx + std::sin(a) * inner, cy - std::cos(a) * inner,
               cx + std::sin(a) * outer, cy - std::cos(a) * outer,
               i % 3 == 0 ? 1.5f : 1.f);
  }
  for (int i = 0; i < (large ? 12 : 5); ++i) {
    float yy = face.getY() + face.getHeight() * (i + 1) / (large ? 13.f : 6.f);
    float dy = yy - cy, rr = face.getWidth() * .5f;
    float xx = std::sqrt(juce::jmax(0.f, rr * rr - dy * dy));
    g.setColour(juce::Colour(0xffc1c6ca).withAlpha(.025f));
    g.drawLine(cx - xx + 4, yy, cx + xx - 4, yy, .7f);
  }
  if (large) {
    g.setColour(juce::Colour(0xff747b81).withAlpha(.35f));
    juce::Path crack;
    crack.startNewSubPath(cx + 22, cy - 80);
    crack.lineTo(cx + 12, cy - 48);
    crack.lineTo(cx + 25, cy - 31);
    crack.lineTo(cx + 14, cy - 11);
    g.strokePath(crack, juce::PathStrokeType(1.4f));
    g.setColour(hot.withAlpha(position * .45f));
    g.strokePath(crack, juce::PathStrokeType(1));
    g.setColour(juce::Colour(0xff9a9fa2));
    g.setFont(
        juce::Font(juce::FontOptions("Bahnschrift", 11, juce::Font::plain)));
    g.setFont(
        juce::Font(juce::FontOptions("Bahnschrift", 36, juce::Font::bold)));
    g.setColour(steel());
    g.drawText(juce::String(slider.getValue(), 0) + "%",
               juce::Rectangle<float>(cx - 75, cy - 18, 150, 45),
               juce::Justification::centred);
    g.setFont(
        juce::Font(juce::FontOptions("Bahnschrift", 11, juce::Font::plain)));
    g.setColour(juce::Colour(0xff9a9fa2));
    g.drawText("D / STRUCT", juce::Rectangle<float>(cx - 55, cy + 27, 110, 18),
               juce::Justification::centred);
  }
  float r = face.getWidth() * .5f;
  g.setColour(hot.withAlpha(.22f));
  g.drawLine(cx + std::sin(angle) * (r * .52f),
             cy - std::cos(angle) * (r * .52f),
             cx + std::sin(angle) * (r * .90f),
             cy - std::cos(angle) * (r * .90f), large ? 7.f : 5.f);
  g.setColour(juce::Colour(0xffffe5cc));
  g.drawLine(cx + std::sin(angle) * (r * .52f),
             cy - std::cos(angle) * (r * .52f),
             cx + std::sin(angle) * (r * .90f),
             cy - std::cos(angle) * (r * .90f), large ? 3.f : 2.f);
}
void DisruptionLookAndFeel::drawButtonBackground(juce::Graphics &g,
                                                 juce::Button &b,
                                                 const juce::Colour &,
                                                 bool over, bool down) {
  auto r = b.getLocalBounds().toFloat().reduced(.5f);
  bool selected = b.getToggleState();
  g.setGradientFill(juce::ColourGradient(
      selected ? juce::Colour(0xff56291e) : juce::Colour(0xff30363b), 0, 0,
      selected ? juce::Colour(0xff231812) : juce::Colour(0xff171b1e), 0,
      r.getBottom(), false));
  g.fillRoundedRectangle(r, 3);
  g.setColour(selected ? orange()
                       : juce::Colour(over || down ? 0xff95999a : 0xff51595e));
  g.drawRoundedRectangle(r, 3, selected ? 1.4f : 1.f);
  if (selected) {
    g.setColour(orange());
    g.fillRect(r.getX() + 12, r.getBottom() - 4, r.getWidth() - 24, 2.f);
  }
  if (b.getProperties().contains("engine")) {
    int mode = static_cast<int>(b.getProperties()["engine"]);
    g.setColour((selected ? orange() : steel()).withAlpha(.28f));
    for (int i = 0; i <= mode; ++i)
      g.drawLine(r.getRight() - 22 + i * 3.f, r.getY() + 7,
                 r.getRight() - 25 + i * 3.f, r.getY() + 14, 1);
  }
}
void DisruptionLookAndFeel::drawButtonText(juce::Graphics &g,
                                           juce::TextButton &b, bool, bool) {
  g.setColour((b.getToggleState() ? juce::Colour(0xffffd5b5) : steel())
                  .withAlpha(b.isEnabled() ? 1.f : .3f));
  g.setFont(juce::Font(juce::FontOptions(
      "Bahnschrift", b.getProperties().contains("engine") ? 17.f : 13.f,
      juce::Font::bold)));
  g.drawText(b.getButtonText(), b.getLocalBounds().reduced(4),
             juce::Justification::centred);
}
void DisruptionLookAndFeel::drawToggleButton(juce::Graphics &g,
                                             juce::ToggleButton &b, bool over,
                                             bool down) {
  drawButtonBackground(g, b, {}, over, down);
  bool on = b.getToggleState();
  g.setColour(on ? orange() : juce::Colour(0xff6b7176));
  g.fillEllipse(9, static_cast<float>(b.getHeight()) * .5f - 3, 6, 6);
  g.setColour(steel());
  g.setFont(juce::Font(juce::FontOptions("Bahnschrift", 12, juce::Font::bold)));
  g.drawText(b.getButtonText() + (on ? " ON" : " OFF"), 24, 0,
             b.getWidth() - 26, b.getHeight(),
             juce::Justification::centredLeft);
}
void DisruptionLookAndFeel::drawComboBox(juce::Graphics &g, int w, int h, bool,
                                         int, int, int, int, juce::ComboBox &) {
  auto r = juce::Rectangle<float>(.5f, .5f, w - 1.f, h - 1.f);
  g.setColour(juce::Colour(0xff0d0f11));
  g.fillRoundedRectangle(r, 3);
  g.setColour(juce::Colour(0xff4e565b));
  g.drawRoundedRectangle(r, 3, 1);
  g.setColour(orange());
  juce::Path p;
  p.addTriangle(w - 19.f, h * .43f, w - 9.f, h * .43f, w - 14.f, h * .62f);
  g.fillPath(p);
}
} // namespace disruption::ui
