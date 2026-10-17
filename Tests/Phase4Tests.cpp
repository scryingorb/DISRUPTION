#include "PluginProcessor.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
extern thread_local bool track;
extern int allocations;
namespace {
int checks = 0;
constexpr double pi = 3.141592653589793;
void ck(bool v, const char *m) {
  ++checks;
  if (!v)
    throw std::runtime_error(m);
}
juce::File root() {
  return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
      .getParentDirectory()
      .getParentDirectory()
      .getParentDirectory();
}
juce::File docs() {
  auto f = root().getChildFile("Docs/Phase4");
  ck(f.createDirectory().wasOk(), "Phase4 dir");
  return f;
}
void run(disruption::DisruptionEngine &e, juce::AudioBuffer<float> &b,
         const disruption::Controls &c) {
  track = true;
  e.process(b, c);
  track = false;
}
float source(int type, int i, double rate) {
  double t = i / rate;
  float s = static_cast<float>(std::sin(2 * pi * 137 * t));
  switch (type) {
  case 0:
    return 0;
  case 1:
    return 1e-8f * s;
  case 2:
    return s;
  case 3:
    return s > 0 ? 1 : -1;
  case 4:
    return static_cast<float>(2 * std::fmod(137 * t, 1) - 1);
  case 5:
    return i == 0 ? 1 : 0;
  case 6:
    return 1;
  case 7:
    return 1000 * s;
  case 8:
    return static_cast<float>(.6 * std::sin(2 * pi * 55 * t) +
                              .2 * std::sin(2 * pi * 110 * t));
  case 9:
    return static_cast<float>(.3 * std::sin(2 * pi * 180 * t) +
                              .15 * std::sin(2 * pi * 540 * t) +
                              .1 * std::sin(2 * pi * 1260 * t));
  default:
    return static_cast<float>(std::sin(2 * pi * (20 * t + 10000 * t * t)));
  }
}
void response() {
  std::ofstream csv(docs()
                        .getChildFile("frequency-response.csv")
                        .getFullPathName()
                        .toStdString());
  csv << "rate,stage,amount,hz,gain_db,phase_deg\n";
  for (double rate : {44100., 48000., 88200., 96000.})
    for (int stage = 0; stage < 2; ++stage)
      for (float a : (stage == 0 ? std::vector<float>{-100, -50, 0, 50, 100}
                                 : std::vector<float>{0, 25, 50, 75, 100}))
        for (double f : {10., 20., 40., 80., 160., 350., 700., 1200., 2500.,
                         5000., 10000., 18000.}) {
          disruption::ToneProcessor tone;
          disruption::BodyProcessor body;
          tone.prepare(rate);
          body.prepare(rate);
          tone.beginFrame(a);
          body.beginFrame(a);
          double re = 0, im = 0, in = 0;
          int n = static_cast<int>(rate * 2);
          for (int i = 0; i < n; ++i) {
            float x =
                static_cast<float>(.001 * std::sin(2 * pi * f * i / rate));
            float y =
                stage == 0 ? tone.process(x, 0, a) : body.process(x, 0, a);
            ck(std::isfinite(y), "response finite");
            if (i >= n / 2) {
              re += y * std::sin(2 * pi * f * i / rate);
              im += y * std::cos(2 * pi * f * i / rate);
              in += x * std::sin(2 * pi * f * i / rate);
            }
          }
          double gain = 20 * std::log10(std::hypot(re, im) / in);
          ck(gain < 6.1 && gain > -6.1, "response bound");
          if (stage == 1)
            ck(gain >= -0.01, "BODY phase cancellation");
          if (a == 0)
            ck(std::abs(gain) < 1e-5, "neutral frequency response");
          csv << rate << "," << (stage == 0 ? "TONE" : "BODY") << "," << a
              << "," << f << "," << gain << "," << std::atan2(im, re) * 180 / pi
              << "\n";
        }
  ck(csv.good(), "response csv");
}
void matrix() {
  std::ofstream csv(docs()
                        .getChildFile("interaction-levels.csv")
                        .getFullPathName()
                        .toStdString());
  csv << "rate,channels,mode,tone,body,crush,signal,peak,rms\n";
  double peak = 0;
  for (double rate : {44100., 48000., 88200., 96000.})
    for (int channels : {1, 2})
      for (int mode = 0; mode < 5; ++mode)
        for (int setting = 0; setting < 6; ++setting)
          for (int type = 0; type < 11; ++type) {
            disruption::Controls c;
            c.mode = mode;
            c.drive = setting >= 4 ? 36 : 12;
            c.damage = setting >= 4 ? 100 : 65;
            c.tone = setting == 0   ? -50
                     : setting == 1 ? 50
                     : setting == 4 ? -100
                     : setting == 5 ? 100
                                    : 0;
            c.body = setting == 2 ? 75 : setting >= 4 ? 100 : 0;
            c.crush = setting == 3 ? 70 : setting >= 4 ? 100 : 0;
            disruption::DisruptionEngine e;
            e.prepare(rate, c);
            juce::AudioBuffer<float> b(channels, 128);
            double energy = 0, p = 0;
            for (int k = 0; k < 40; ++k) {
              for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < 128; ++i)
                  b.setSample(ch, i,
                              source(type, k * 128 + i, rate) * (ch ? -1 : 1));
              run(e, b, c);
              for (int ch = 0; ch < channels; ++ch)
                for (int i = 0; i < 128; ++i) {
                  float x = b.getSample(ch, i);
                  ck(std::isfinite(x) && std::abs(x) < 10,
                     "Phase4 interaction safety");
                  if (type == 0)
                    ck(x == 0, "Phase4 silence");
                  if (ch == 1 && (mode == 1 || mode == 3))
                    ck(std::abs(x + b.getSample(0, i)) < 1e-5,
                       "opposite stereo independence");
                  p = std::max(p, static_cast<double>(std::abs(x)));
                  energy += x * x;
                }
            }
            peak = std::max(peak, p);
            csv << rate << "," << channels << "," << mode << "," << c.tone
                << "," << c.body << "," << c.crush << "," << type << "," << p
                << "," << std::sqrt(energy / (5120 * channels)) << "\n";
          }
  std::cout << "Phase4 matrix peak " << peak << "\n";
  ck(csv.good(), "matrix csv");
}
void clockTests() {
  std::ofstream csv(docs()
                        .getChildFile("global-crush-mapping.csv")
                        .getFullPathName()
                        .toStdString());
  csv << "rate,amount,bits,effective_rate,captures\n";
  for (double rate : {44100., 48000., 88200., 96000.})
    for (float a : {0.f, 25.f, 50.f, 75.f, 100.f}) {
      disruption::CrushProcessor p;
      p.prepare(rate);
      int captures = 0;
      for (int i = 0; i < static_cast<int>(rate); ++i) {
        p.beginFrame(a);
        captures += p.didCaptureThisFrame();
        float x = .37f * std::sin(static_cast<float>(i * .03));
        float l = p.process(x, 0, a), r = p.process(-x, 1, a);
        ck(l == -r, "global crush stereo clock");
        if (a == 0)
          ck(l == x, "global crush transparent");
      }
      ck(std::abs(captures - p.getEffectiveRate()) <= 1.1,
         "global crush physical rate");
      csv << rate << "," << a << "," << p.effectiveBits(a) << ","
          << p.getEffectiveRate() << "," << captures << "\n";
    }
  ck(csv.good(), "clock csv");
}
void automation() {
  std::ofstream csv(
      docs().getChildFile("automation.csv").getFullPathName().toStdString());
  csv << "rate,mode,max_step,first_boundary_step\n";
  for (double rate : {44100., 48000., 88200., 96000.})
    for (int mode = 0; mode < 5; ++mode) {
      disruption::Controls c;
      c.mode = mode;
      c.drive = 12;
      c.damage = 65;
      disruption::DisruptionEngine e;
      e.prepare(rate, c);
      juce::AudioBuffer<float> b(2, 64);
      float last = 0;
      double step = 0, boundary = 0;
      for (int k = 0; k < 300; ++k) {
        if (k % 20 == 0) {
          c.tone = k % 40 ? -100 : 100;
          c.body = k % 40 ? 0 : 100;
          c.crush = k % 40 ? 0 : 100;
        }
        if (k % 30 == 0) {
          c.drive = k % 60 ? 0 : 36;
          c.damage = k % 60 ? 0 : 100;
          c.mode = (mode + k / 30) % 5;
          c.mix = k % 60 ? 0 : 100;
          c.output = k % 60 ? -24 : 12;
          c.bypass = k % 60 != 0;
        }
        for (int ch = 0; ch < 2; ++ch)
          for (int i = 0; i < 64; ++i)
            b.setSample(ch, i, .1f);
        run(e, b, c);
        for (int i = 0; i < 64; ++i) {
          float x = b.getSample(0, i);
          ck(std::isfinite(x) && std::abs(x) < 12, "automated finite bounded");
          double jump = std::abs(x - last);
          step = std::max(step, jump);
          if (i == 0)
            boundary = std::max(boundary, jump);
          last = x;
        }
      }
      ck(step < 12, "automation exceeded physical full-stack range");
      csv << rate << "," << mode << "," << step << "," << boundary << "\n";
    }
  for (double rate : {44100., 48000., 88200., 96000.}) {
    disruption::Controls c;
    c.tone = -100;
    c.body = 100;
    c.crush = 100;
    c.mix = 0;
    c.drive = 36;
    c.damage = 100;
    disruption::DisruptionEngine e;
    e.prepare(rate, c);
    juce::AudioBuffer<float> b(2, 128);
    b.clear();
    for (int i = 0; i < 128; ++i)
      b.setSample(0, i, .2f);
    run(e, b, c);
    for (int i = 0; i < 128; ++i) {
      ck(b.getSample(0, i) == .2f, "full stage dry leak");
      ck(b.getSample(1, i) == 0, "channel leak");
    }
  }
}
std::unique_ptr<juce::AudioPluginInstance>
load(juce::AudioPluginFormatManager &m, const juce::File &f) {
  juce::OwnedArray<juce::PluginDescription> d;
  m.getFormat(0)->findAllTypesForFile(d, f.getFullPathName());
  ck(d.size() == 1, "Phase4 discovery");
  juce::String err;
  auto p = m.createPluginInstance(*d[0], 48000, 128, err);
  ck(p != nullptr, "Phase4 host load");
  return p;
}
void configure(juce::AudioProcessor &p, int mode, int channels, double rate,
               float drive = .5f, float damage = .75f) {
  for (auto *x : p.getParameters())
    x->setValueNotifyingHost(0);
  p.getParameters()[0]->setValueNotifyingHost(mode / 4.f);
  p.getParameters()[1]->setValueNotifyingHost(drive);
  p.getParameters()[2]->setValueNotifyingHost(damage);
  p.getParameters()[3]->setValueNotifyingHost(.5f);
  p.getParameters()[6]->setValueNotifyingHost(1);
  p.getParameters()[7]->setValueNotifyingHost(2.f / 3);
  juce::AudioProcessor::BusesLayout l;
  auto s = channels == 1 ? juce::AudioChannelSet::mono()
                         : juce::AudioChannelSet::stereo();
  l.inputBuses.add(s);
  l.outputBuses.add(s);
  ck(p.setBusesLayout(l), "phase4 layout");
  p.prepareToPlay(rate, 128);
}
void host(const char *path) {
  juce::AudioPluginFormatManager m;
  m.addFormat(std::make_unique<juce::VST3PluginFormat>());
  auto old =
      load(m, root().getChildFile("build/Phase3Baseline/DISRUPTION.vst3"));
  auto now = load(m, juce::File(path));
  double worst = 0;
  for (double rate : {44100., 48000., 88200., 96000.})
    for (int ch : {1, 2})
      for (int mode = 0; mode < 5; ++mode)
        for (float drive : {0.f, .5f, 1.f})
          for (float damage : {0.f, .5f, 1.f}) {
            configure(*old, mode, ch, rate, drive, damage);
            configure(*now, mode, ch, rate, drive, damage);
            std::array<std::array<float,256>,2> delay {}; int pos=0; const int latency=now->getLatencySamples();
            juce::AudioBuffer<float> a(ch, 128), b(ch, 128);
            juce::MidiBuffer midi;
            for (int k = 0; k < 64; ++k) {
              for (int channel = 0; channel < ch; ++channel)
                for (int i = 0; i < 128; ++i)
                  a.setSample(channel, i,
                              source(9, k * 128 + i, rate) *
                                  (channel ? -1 : 1));
              b.makeCopyOf(a, true);
              old->processBlock(a, midi);
              now->processBlock(b, midi);
              for (int channel = 0; channel < ch; ++channel)
                for (int i = 0; i < 128; ++i) {
                  int index=(pos+i)%latency; float delayed=delay[channel][index]; delay[channel][index]=a.getSample(channel,i);
                  double delta = std::abs(delayed-b.getSample(channel,i));
                  worst = std::max(worst, delta);
                  ck(delta < 1e-6, "Phase3 neutral audio regression");
                }
              pos=(pos+128)%latency;
            }
            old->releaseResources();
            now->releaseResources();
          }
  std::cout << "Phase3 all-five neutral archived DLL difference " << worst
            << "\n";
  std::ofstream csv(
      docs().getChildFile("cpu-benchmark.csv").getFullPathName().toStdString());
  csv << "mode,configuration,median_us,p95_us,core_percent\n";
  for (int mode = 0; mode < 5; ++mode)
    for (int stage = -1; stage < 5; ++stage) {
      juce::AudioProcessor *p = stage == -1 ? old.get() : now.get();
      configure(*p, mode, 2, 48000);
      if (stage == 1 || stage == 4)
        p->getParameters()[3]->setValueNotifyingHost(.75f);
      if (stage == 2 || stage == 4)
        p->getParameters()[4]->setValueNotifyingHost(.6f);
      if (stage == 3 || stage == 4)
        p->getParameters()[5]->setValueNotifyingHost(.6f);
      juce::AudioBuffer<float> b(2, 64);
      juce::MidiBuffer midi;
      std::vector<double> times;
      times.reserve(1000);
      for (int k = 0; k < 1200; ++k) {
        for (int ch = 0; ch < 2; ++ch)
          for (int i = 0; i < 64; ++i)
            b.setSample(ch, i, source(9, k * 64 + i, 48000));
        auto start = std::chrono::steady_clock::now();
        p->processBlock(b, midi);
        double us = std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - start)
                        .count();
        if (k >= 200)
          times.push_back(us);
      }
      std::sort(times.begin(), times.end());
      csv << mode << ","
          << (stage == -1  ? "Phase3"
              : stage == 0 ? "neutral"
              : stage == 1 ? "tone"
              : stage == 2 ? "body"
              : stage == 3 ? "crush"
                           : "all")
          << "," << times[500] << "," << times[950] << "," << times[500] * .075
          << "\n";
      p->releaseResources();
    }
  ck(csv.good(), "CPU csv");
}

void stageTransitions() {
  std::ofstream csv(docs()
                        .getChildFile("stage-transition-residual.csv")
                        .getFullPathName()
                        .toStdString());
  csv << "rate,mode,stage,direction,first_residual,max_residual_step\n";
  for (double rate : {44100., 48000., 88200., 96000.})
    for (int mode = 0; mode < 5; ++mode)
      for (int stage = 0; stage < 3; ++stage)
        for (int direction : {0, 1}) {
          disruption::Controls c;
          c.mode = mode;
          c.drive = 6;
          c.damage = 50;
          if (direction) {
            if (stage == 0)
              c.tone = -100;
            if (stage == 1)
              c.body = 100;
            if (stage == 2)
              c.crush = 100;
          }
          disruption::DisruptionEngine a, b;
          a.prepare(rate, c);
          b.prepare(rate, c);
          juce::AudioBuffer<float> x(1, 64), y(1, 64);
          for (int k = 0; k < 200; ++k) {
            for (int i = 0; i < 64; ++i)
              x.setSample(0, i,
                          stage == 1 ? static_cast<float>(
                                           .1 * std::sin(2 * pi * 130 *
                                                         (k * 64 + i) / rate))
                                     : .1f);
            y.makeCopyOf(x, true);
            run(a, x, c);
            run(b, y, c);
          }
          auto changed = c;
          if (stage == 0)
            changed.tone = direction ? 100 : -100;
          if (stage == 1)
            changed.body = direction ? 0 : 100;
          if (stage == 2)
            changed.crush = direction ? 0 : 100;
          double first = 0, step = 0;
          float last = 0;
          for (int k = 0; k < 40; ++k) {
            for (int i = 0; i < 64; ++i)
              x.setSample(0, i,
                          stage == 1
                              ? static_cast<float>(
                                    .1 * std::sin(2 * pi * 130 *
                                                  ((200 + k) * 64 + i) / rate))
                              : .1f);
            y.makeCopyOf(x, true);
            run(a, x, changed);
            run(b, y, c);
            for (int i = 0; i < 64; ++i) {
              float residual = x.getSample(0, i) - y.getSample(0, i);
              if (k == 0 && i == 0)
                first = std::abs(residual);
              step = std::max(step,
                              static_cast<double>(std::abs(residual - last)));
              last = residual;
            }
          }
          ck(first < .005, "new stage first sample control click");
          ck(step < .01, "new stage residual control discontinuity");
          csv << rate << "," << mode << "," << stage << "," << direction << ","
              << first << "," << step << "\n";
        }
  ck(csv.good(), "transition residual CSV");
  // BODY is exactly DC-neutral after extraction settles and its parallel
  // addition is bounded.
  disruption::BodyProcessor body;
  body.prepare(48000);
  body.beginFrame(100);
  float end = 0;
  for (int i = 0; i < 480000; ++i)
    end = body.process(1, 0, 100);
  ck(std::abs(end - 1) < 1e-6, "BODY steady DC boost");
  for (float amplitude : {.001f, .1f, 1.f, 10.f, 1000.f}) {
    body.reset();
    double added = 0;
    for (int i = 0; i < 48000; ++i) {
      float x =
          amplitude * static_cast<float>(std::sin(2 * pi * 80 * i / 48000));
      float y = body.process(x, 0, 100);
      added = std::max(added, static_cast<double>(std::abs(y - x)));
    }
    ck(added <= 1.5001, "BODY saturation contribution bound");
  }
}

void levelsAndMix() {
  std::ofstream csv(docs()
                        .getChildFile("level-increases.csv")
                        .getFullPathName()
                        .toStdString());
  csv << "mode,signal,tone,body,crush,reference_rms,processed_rms,rms_increase_"
         "db,reference_peak,processed_peak,peak_increase_db\n";
  for (int mode = 0; mode < 5; ++mode)
    for (int type : {2, 8, 9})
      for (int setting = 0; setting < 9; ++setting) {
        disruption::Controls c;
        c.mode = mode;
        c.drive = 12;
        c.damage = setting >= 7 ? 100.0f : 65.0f;
        c.drive = setting >= 7 ? 36.0f : 12.0f;
        auto d = c;
        d.tone = setting == 7   ? 100.0f
                 : setting == 8 ? -100.0f
                 : setting == 0 ? -100
                 : setting == 1 ? 100
                 : setting >= 4 ? -50
                                : 0;
        d.body = setting >= 7   ? 100.0f
                 : setting == 2 ? 100
                 : setting >= 4 ? 75
                                : 0;
        d.crush = setting >= 7   ? 100.0f
                  : setting == 3 ? 100
                  : setting == 5 ? 75
                  : setting == 6 ? 100
                                 : 0;
        disruption::DisruptionEngine a, b;
        a.prepare(48000, c);
        b.prepare(48000, d);
        juce::AudioBuffer<float> x(1, 128), y(1, 128);
        double energy = 0, other = 0, peak = 0, p = 0;
        for (int k = 0; k < 375; ++k) {
          for (int i = 0; i < 128; ++i)
            x.setSample(0, i, .5f * source(type, k * 128 + i, 48000));
          y.makeCopyOf(x, true);
          run(a, x, c);
          run(b, y, d);
          if (k > 100)
            for (int i = 0; i < 128; ++i) {
              float v = x.getSample(0, i), w = y.getSample(0, i);
              energy += v * v;
              other += w * w;
              peak = std::max(peak, static_cast<double>(std::abs(v)));
              p = std::max(p, static_cast<double>(std::abs(w)));
            }
        }
        ck(energy > 0 && other > 0, "levels audible");
        double gain = 10 * std::log10(other / energy);
        ck(gain < 12.1, "unreasonable stage RMS gain");
        csv << mode << "," << type << "," << d.tone << "," << d.body << ","
            << d.crush << "," << std::sqrt(energy / 35072) << ","
            << std::sqrt(other / 35072) << "," << gain << "," << peak << ","
            << p << "," << 20 * std::log10(p / peak) << "\n";
      }
  ck(csv.good(), "level increases CSV");
  // First impulse has zero final DC history: independently assemble the whole
  // wet path.
  for (int mode = 0; mode < 5; ++mode)
    for (float mix : {0.f, 25.f, 50.f, 100.f}) {
      disruption::Controls c;
      c.mode = mode;
      c.drive = 12;
      c.damage = 65;
      c.tone = -35;
      c.body = 60;
      c.crush = 75;
      c.mix = mix;
      disruption::DisruptionEngine e;
      e.prepare(48000, c);
      disruption::BurnEngine burn;
      disruption::BreakEngine breaking;
      disruption::RipEngine rip;
      disruption::CrushEngine crushing;
      disruption::ChaosEngine chaos;
      std::array<disruption::ModeEngine *, 5> modes{
          {&burn, &breaking, &rip, &crushing, &chaos}};
      for (auto *m : modes) {
        m->prepare(48000);
        m->beginFrame(c.damage);
      }
      float wet = modes[mode]->process(
          .2f * juce::Decibels::decibelsToGain(c.drive), 0, c.damage);
      disruption::ToneProcessor tone;
      disruption::BodyProcessor body;
      disruption::CrushProcessor crush;
      tone.prepare(48000);
      body.prepare(48000);
      crush.prepare(48000);
      tone.beginFrame(c.tone);
      body.beginFrame(c.body);
      crush.beginFrame(c.crush);
      wet = body.process(
          tone.process(crush.process(wet, 0, c.crush), 0, c.tone), 0, c.body);
      juce::AudioBuffer<float> b(1, 1);
      b.setSample(0, 0, .2f);
      run(e, b, c);
      ck(std::abs(b.getSample(0, 0) -
                  (.2f * (1 - mix * .01f) + wet * mix * .01f)) < 1e-6,
         "MIX entire wet chain integration");
    }
}
void evidence() {
  std::ofstream hold(docs()
                         .getChildFile("global-crush-hold.csv")
                         .getFullPathName()
                         .toStdString());
  hold << "amount,frame,input,captured,output\n";
  for (float amount : {0.f, 25.f, 50.f, 75.f, 100.f}) {
    disruption::CrushProcessor p;
    p.prepare(48000);
    for (int i = 0; i < 2048; ++i) {
      float x = .8f * static_cast<float>(std::sin(2 * pi * 437 * i / 48000));
      p.beginFrame(amount);
      hold << amount << "," << i << "," << x << "," << p.didCaptureThisFrame()
           << "," << p.process(x, 0, amount) << "\n";
    }
  }
  ck(hold.good(), "hold CSV");
  // Compare each stereo channel to its own mono processor, including asymmetry.
  for (int mode = 0; mode < 5; ++mode) {
    disruption::Controls c;
    c.mode = mode;
    c.tone = 50;
    c.body = 75;
    c.crush = 70;
    c.drive = 12;
    c.damage = 65;
    disruption::DisruptionEngine stereo, left, right;
    stereo.prepare(48000, c);
    left.prepare(48000, c);
    right.prepare(48000, c);
    juce::AudioBuffer<float> b(2, 128), l(1, 128), r(1, 128);
    for (int k = 0; k < 40; ++k) {
      for (int i = 0; i < 128; ++i) {
        float x = source(9, k * 128 + i, 48000);
        b.setSample(0, i, x);
        b.setSample(1, i, -x);
        l.setSample(0, i, x);
        r.setSample(0, i, -x);
      }
      run(stereo, b, c);
      run(left, l, c);
      run(right, r, c);
      for (int i = 0; i < 128; ++i) {
        ck(b.getSample(0, i) == l.getSample(0, i), "stereo left vs mono");
        ck(b.getSample(1, i) == r.getSample(0, i), "stereo right vs mono");
      }
    }
  }
  // Even dedicated maximum CRUSH receives extra non-gain digital changes.
  for (float damage : {25.f, 50.f, 100.f}) {
    disruption::Controls c;
    c.mode = 3;
    c.damage = damage;
    auto d = c;
    d.crush = 100;
    disruption::DisruptionEngine a, b;
    a.prepare(48000, c);
    b.prepare(48000, d);
    juce::AudioBuffer<float> x(1, 128), y(1, 128);
    double difference = 0, energy = 0, cross = 0, other = 0;
    for (int k = 0; k < 100; ++k) {
      for (int i = 0; i < 128; ++i)
        x.setSample(0, i, source(9, k * 128 + i, 48000));
      y.makeCopyOf(x, true);
      run(a, x, c);
      run(b, y, d);
      for (int i = 0; i < 128; ++i) {
        double v = x.getSample(0, i), w = y.getSample(0, i);
        difference += (v - w) * (v - w);
        energy += v * v;
        other += w * w;
        cross += v * w;
      }
    }
    ck(difference > 0.01, "cascade redundant");
    ck(other - cross * cross / energy > 0.001, "cascade only gain");
  }
  juce::WavAudioFormat format;
  auto reader = std::unique_ptr<juce::AudioFormatReader>(format.createReaderFor(
      root()
          .getChildFile("Docs/Phase2/Listening-Source.wav")
          .createInputStream()
          .release(),
      true));
  ck(reader != nullptr, "Phase4 source");
  int frames = static_cast<int>(reader->lengthInSamples);
  juce::AudioBuffer<float> sourceAudio(2, frames);
  ck(reader->read(&sourceAudio, 0, frames, 0, true, true),
     "Phase4 source read");
  auto write = [&](const juce::String &name, juce::AudioBuffer<float> &audio) {
    auto file = docs().getChildFile(name);
    auto stream = std::make_unique<juce::FileOutputStream>(file);
    ck(stream->openedOk(), "Phase4 WAV open");
    stream->setPosition(0);
    stream->truncate();
    std::unique_ptr<juce::OutputStream> out = std::move(stream);
    auto writer = format.createWriterFor(
        out,
        juce::AudioFormatWriterOptions{}
            .withSampleRate(reader->sampleRate)
            .withNumChannels(2)
            .withBitsPerSample(32)
            .withSampleFormat(
                juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    ck(writer &&
           writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()),
       "Phase4 WAV write");
    writer.reset();
    auto verify = std::unique_ptr<juce::AudioFormatReader>(
        format.createReaderFor(file.createInputStream().release(), true));
    ck(verify && verify->usesFloatingPointData &&
           verify->lengthInSamples == audio.getNumSamples(),
       "Phase4 float WAV verify");
    juce::Range<float> ranges[2];
    verify->readMaxLevels(0, audio.getNumSamples(), ranges, 2);
    float peak = 0;
    for (auto range : ranges)
      peak = std::max(
          peak, std::max(std::abs(range.getStart()), std::abs(range.getEnd())));
    ck(std::abs(peak - audio.getMagnitude(0, audio.getNumSamples())) < 1e-6,
       "Phase4 float WAV peak");
  };
  std::ofstream csv(docs()
                        .getChildFile("listening-levels.csv")
                        .getFullPathName()
                        .toStdString());
  csv << "name,mode,drive,damage,tone,body,crush,raw_rms,raw_peak,match_gain_"
         "db\n";
  std::vector<std::pair<juce::String, disruption::Controls>> settings;
  for (float a : {-100.f, -50.f, 0.f, 50.f, 100.f}) {
    disruption::Controls c;
    c.drive = 12;
    c.damage = 65;
    c.tone = a;
    settings.push_back({"Tone-" + juce::String(a), c});
  }
  for (float a : {0.f, 25.f, 50.f, 75.f, 100.f}) {
    disruption::Controls c;
    c.drive = 12;
    c.damage = 65;
    c.body = a;
    settings.push_back({"Body-" + juce::String(a), c});
  }
  for (float a : {0.f, 25.f, 50.f, 75.f, 100.f}) {
    disruption::Controls c;
    c.drive = 12;
    c.damage = 65;
    c.crush = a;
    settings.push_back({"GlobalCrush-" + juce::String(a), c});
  }
  for (int mode : {0, 1, 2, 4}) {
    disruption::Controls c;
    c.mode = mode;
    c.drive = 12;
    c.damage = 65;
    c.body = mode == 2 ? 0 : 60;
    c.tone = mode == 1 || mode == 4 ? -35 : mode == 2 ? 35 : 0;
    c.crush = mode == 0 ? 20 : mode == 2 ? 55 : 0;
    settings.push_back({"Stack-" + juce::String(mode), c});
  }
  for (auto &setting : settings) {
    auto c = setting.second;
    disruption::DisruptionEngine e;
    e.prepare(reader->sampleRate, c);
    juce::AudioBuffer<float> audio(sourceAudio);
    run(e, audio, c);
    audio.applyGainRamp(0, 480, 0, 1);
    audio.applyGainRamp(frames - 480, 480, 1, 0);
    double energy = 0;
    for (int ch = 0; ch < 2; ++ch)
      for (int i = 0; i < frames; ++i)
        energy += std::pow(audio.getSample(ch, i), 2);
    double rms = std::sqrt(energy / (2 * frames));
    ck(rms > 0, "Phase4 render audible");
    float peak = audio.getMagnitude(0, frames);
    float gain = static_cast<float>(.1 / rms);
    csv << setting.first << "," << c.mode << "," << c.drive << "," << c.damage
        << "," << c.tone << "," << c.body << "," << c.crush << "," << rms << ","
        << peak << "," << 20 * std::log10(gain) << "\n";
    write(setting.first + "-Raw.wav", audio);
    audio.applyGain(gain);
    ck(audio.getMagnitude(0, frames) < 1, "matched render peak");
    write(setting.first + "-RMS-matched.wav", audio);
  }
  ck(csv.good(), "Phase4 listening CSV");
}

} // namespace
int runPhase4Tests(const char *path) {
  response();
  clockTests();
  matrix();
  automation();
  stageTransitions();
  host(path);
  levelsAndMix();
  evidence();
  ck(allocations == 0, "Phase4 callback allocation");
  std::cout << "Phase4: " << checks << " assertions passed\n";
  return checks;
}
