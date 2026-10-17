#include "DisruptionVisualizer.h"
namespace disruption::ui {
void DisruptionVisualizer::update(float amount, int engine, bool bypass) {
  damage = amount * .01f;
  mode = engine;
  bypassed = bypass;
  ++frame;
  int n = bridge.drain(incoming.data(), static_cast<int>(incoming.size()));
  peak *= .88f;
  if (n > 0) {
    int keep = juce::jmin(n, static_cast<int>(wave.size()));
    if (keep < static_cast<int>(wave.size()))
      std::move(wave.begin() + keep, wave.end(), wave.begin());
    for (int i = 0; i < keep; ++i) {
      float v = incoming[n - keep + i];
      wave[wave.size() - keep + i] = v;
      peak = juce::jmax(peak, std::abs(v));
    }
  } else
    for (auto &v : wave)
      v *= .90f;
  for (auto &p : particles) {
    if (p.life > 0) {
      p.x += p.vx;
      p.y += p.vy;
      p.vy += .045f;
      p.life -= .06f;
    }
  }
  if (!bypassed && damage > .75f && peak > .04f && frame % 3 == 0) {
    for (auto &p : particles)
      if (p.life <= 0) {
        float seed = static_cast<float>((frame * 17) % 97) / 97.f;
        p = {getWidth() * (.3f + .4f * seed),
             getHeight() * .5f + (seed - .5f) * 65, (seed - .5f) * 3,
             -.8f - seed, 1};
        break;
      }
  }
  repaint();
}
void DisruptionVisualizer::paint(juce::Graphics &g) {
  float cy = getHeight() * .5f;
  auto hot = orange().interpolatedWith(juce::Colour(0xffff2f1b), damage * .6f);
  float alpha = (bypassed ? .18f : .65f) * (.35f + .65f * damage);
  g.setColour(juce::Colour(0xff596067).withAlpha(.08f));
  for (int i = 0; i < 11; ++i)
    g.drawVerticalLine(i * getWidth() / 10, 30.f, getHeight() - 30.f);
  g.drawHorizontalLine(static_cast<int>(cy), 0.f,
                       static_cast<float>(getWidth()));
  constexpr int displayPoints = 96;
  float previousX = 0, previousY = cy, last = 0;
  const int increment = mode == 3 ? 3 : 1;
  bool segmentStart = true;
  for (int i = 0; i < displayPoints; i += increment) {
    float v = juce::jlimit(-1.f, 1.f, wave[256 - displayPoints + i]);
    if (mode == 0) {
      v = .6f * v + .4f * last;
      last = v;
    } else if (mode == 1)
      v = juce::jlimit(-.38f, .38f, v) * 1.5f;
    else if (mode == 2)
      v *= v > 0 ? .75f : 1.3f;
    else if (mode == 3)
      v = std::round(v * 12) / 12;
    else if (mode == 4)
      v = std::sin(v * 7) * .4f;
    float x = i * getWidth() / (displayPoints - 1.f),
          y = cy - v * (70 + 60 * damage);
    if (mode == 4 && damage > .5f)
      y += static_cast<float>((i / 13) % 3 - 1) * damage * 4;
    if (!segmentStart) {
      // Individual bounded segments avoid expensive self-intersecting stroked
      // paths.
      float strength = (x > 225 && x < getWidth() - 225) ? 1.f : .16f;
      auto draw = [&](float x1, float y1, float x2, float y2) {
        g.setColour(hot.withAlpha(alpha * .13f * strength));
        g.drawLine(x1, y1, x2, y2, 5);
        g.setColour(hot.withAlpha(alpha * strength));
        g.drawLine(x1, y1, x2, y2, 1.2f + damage);
      };
      if (mode == 3) {
        draw(previousX, previousY, x, previousY);
        draw(x, previousY, x, y);
      } else
        draw(previousX, previousY, x, y);
    }
    previousX = x;
    previousY = y;
    segmentStart = damage > .6f && i % 29 == 27;
  }
  for (auto &p : particles)
    if (p.life > 0) {
      g.setColour(hot.withAlpha(p.life * .65f));
      g.drawLine(p.x, p.y, p.x - p.vx * 3, p.y - p.vy * 3, 1.2f);
    }
}
} // namespace disruption::ui
