#include "PluginEditor.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
extern thread_local bool track;
extern int allocations;
namespace {
int checks = 0;
constexpr double pi = 3.141592653589793;
constexpr int sizes[] = {32, 64, 128, 256, 512, 1024};
constexpr double rates[] = {44100, 48000, 88200, 96000};
constexpr const char *ids[] = {"mode",  "drive", "damage", "tone", "body",
                               "crush", "mix",   "output", "hq",   "bypass"};
using Clock = std::chrono::steady_clock;
void check(bool ok, const char *message) {
  ++checks;
  if (!ok)
    throw std::runtime_error(message);
}
juce::File root() {
  return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
      .getParentDirectory()
      .getParentDirectory()
      .getParentDirectory();
}
juce::File docs() {
  auto f = root().getChildFile("Docs/Phase8");
  check(f.createDirectory().wasOk(), "Phase8 docs");
  return f;
}
std::ofstream csv(const char *name) {
  return std::ofstream(
      docs().getChildFile(name).getFullPathName().toStdString());
}
void set(DisruptionProcessor &p, int i, float value) {
  auto *q = p.parameters.getParameter(ids[i]);
  q->setValueNotifyingHost(q->convertTo0to1(value));
}
void layout(juce::AudioProcessor &p, int channels) {
  juce::AudioProcessor::BusesLayout l;
  auto s = channels == 1 ? juce::AudioChannelSet::mono()
                         : juce::AudioChannelSet::stereo();
  l.inputBuses.add(s);
  l.outputBuses.add(s);
  check(p.setBusesLayout(l), "Phase8 layout");
}
float signal(int kind, int64_t n, double rate) {
  const double t = n / rate, a = std::sin(2 * pi * 173 * t),
               b = std::sin(2 * pi * 551 * t);
  switch (kind) {
  case 0:
    return 0;
  case 1:
    return static_cast<float>(1.e-12 * a);
  case 2:
    return static_cast<float>(a);
  case 3:
    return static_cast<float>(4 * a + 2 * b);
  case 4:
    return .8f;
  case 5:
    return n == 0 ? 1.f : 0.f;
  case 6:
    return a >= 0 ? 1.f : -1.f;
  case 7:
    return static_cast<float>(2 * std::fmod(173 * t, 1.) - 1);
  case 8:
    return static_cast<float>(.8 * std::sin(2 * pi * 43 * t) + .1 * a);
  default:
    return static_cast<float>(
        (.18 * a + .08 * b + .035 * std::sin(2 * pi * 1551 * t)) *
        (.6 + .4 * std::sin(2 * pi * 3 * t)));
  }
}
void fill(juce::AudioBuffer<float> &b, int kind, int64_t n, double rate) {
  for (int c = 0; c < b.getNumChannels(); ++c)
    for (int i = 0; i < b.getNumSamples(); ++i)
      b.setSample(c, i, signal(kind, n + i, rate) * (c ? -.93f : 1.f));
}
double finite(const juce::AudioBuffer<float> &b) {
  double peak = 0;
  for (int c = 0; c < b.getNumChannels(); ++c)
    for (int i = 0; i < b.getNumSamples(); ++i) {
      const float v = b.getSample(c, i);
      check(std::isfinite(v) && std::abs(v) < 100,
            "Phase8 nonfinite/runaway output");
      peak = std::max(peak, static_cast<double>(std::abs(v)));
    }
  return peak;
}
void process(DisruptionProcessor &p, juce::AudioBuffer<float> &b) {
  juce::MidiBuffer midi;
  track = true;
  p.processBlock(b, midi);
  track = false;
}
void pump() {
  MSG m;
  while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
  Sleep(1);
}
void extreme() {
  auto out = csv("extreme-matrix.csv");
  out << "rate,channels,block,mode,hq,tone,input,peak,latency\n";
  int cases = 0;
  double peak = 0;
  for (double rate : rates)
    for (int channels : {1, 2})
      for (int size : sizes)
        for (int mode = 0; mode < 5; ++mode)
          for (int hq : {0, 1})
            for (float tone : {-100.f, 100.f}) {
              disruption::Controls c;
              c.mode = mode;
              c.drive = 36;
              c.damage = 100;
              c.tone = tone;
              c.body = 100;
              c.crush = 100;
              c.mix = 100;
              c.output = 12;
              c.hq = hq != 0;
              disruption::HQProcessor engine;
              engine.prepare(rate, c);
              check(engine.latency() == 60, "Phase8 extreme latency");
              juce::AudioBuffer<float> b(channels, size);
              for (int kind = 0; kind < 10; ++kind) {
                engine.reset();
                double maximum = 0;
                // At least 4096 samples lets every smoothing stage settle,
                // including at 96 kHz.
                for (int n = 0; n < std::max(4096, size * 4); n += size) {
                  fill(b, kind, n, rate);
                  track = true;
                  engine.process(b, c);
                  track = false;
                  maximum = std::max(maximum, finite(b));
                }
                if (kind == 0)
                  check(maximum == 0, "Phase8 extreme silence");
                peak = std::max(peak, maximum);
                ++cases;
                out << rate << ',' << channels << ',' << size << ',' << mode
                    << ',' << hq << ',' << tone << ',' << kind << ',' << maximum
                    << ",60\n";
              }
            }
  std::cout << "Phase8 extreme cases " << cases << "; maximum peak " << peak
            << " (overs permitted)\n";
}
void rateAndBuffer() {
  auto out = csv("sample-rate-buffer.csv");
  out << "rate,channels,preset,hq,bypass,block,latency,latency_ms,peak\n";
  for (double rate : rates)
    for (int channels : {1, 2}) {
      DisruptionProcessor p, restored;
      layout(p, channels);
      layout(restored, channels);
      const auto all = p.presetManager().factoryPresets();
      for (const auto &preset : all)
        for (int hq : {0, 1})
          for (int bypass : {0, 1}) {
            check(p.presetManager().load(preset.id).wasOk(),
                  "Phase8 preset load");
            set(p, 8, static_cast<float>(hq));
            set(p, 9, static_cast<float>(bypass));
            juce::MemoryBlock state;
            p.getStateInformation(state);
            restored.setStateInformation(state.getData(),
                                         static_cast<int>(state.getSize()));
            for (int i = 0; i < 10; ++i)
              check(
                  p.parameters.getRawParameterValue(ids[i])->load() ==
                      restored.parameters.getRawParameterValue(ids[i])->load(),
                  "Phase8 rate state exact");
            p.prepareToPlay(rate, 32);
            restored.prepareToPlay(rate, 32);
            for (int size : sizes) {
              juce::AudioBuffer<float> a(channels, size), b(channels, size);
              double maximum = 0;
              // Change actual callback sizes without prepare, then explicitly
              // reprepare both.
              for (int reprepare : {0, 1}) {
                if (reprepare) {
                  p.prepareToPlay(rate, size);
                  restored.prepareToPlay(rate, size);
                }
                for (int n = 0; n < 4; ++n) {
                  fill(a, 9, n * size, rate);
                  b.makeCopyOf(a, true);
                  process(p, a);
                  process(restored, b);
                  maximum = std::max(maximum, finite(a));
                  for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < size; ++i)
                      check(a.getSample(c, i) == b.getSample(c, i),
                            "Phase8 state restored audio exact");
                }
              }
              check(p.getLatencySamples() == 60 &&
                        restored.getLatencySamples() == 60,
                    "Phase8 rate/block latency");
              out << rate << ',' << channels << ',' << preset.id << ',' << hq
                  << ',' << bypass << ',' << size << ",60," << 60000 / rate
                  << ',' << maximum << '\n';
            }
          }
    }
}
std::unique_ptr<juce::AudioPluginInstance>
load(juce::AudioPluginFormatManager &manager, const juce::File &file) {
  juce::OwnedArray<juce::PluginDescription> descriptions;
  manager.getFormat(0)->findAllTypesForFile(descriptions,
                                            file.getFullPathName());
  check(descriptions.size() == 1, "Phase8 VST3 discovery");
  check(descriptions[0]->manufacturerName == "VoxxLab" &&
            descriptions[0]->name == "DISRUPTION" &&
            !descriptions[0]->isInstrument,
        "Phase8 VST3 identity");
  juce::String error;
  auto p = manager.createPluginInstance(*descriptions[0], 48000, 128, error);
  check(p != nullptr, "Phase8 hosted instantiate");
  return p;
}
void archive(const char *path) {
  juce::AudioPluginFormatManager manager;
  manager.addFormat(std::make_unique<juce::VST3PluginFormat>());
  auto old = load(manager,
                  root().getChildFile("build/Phase7Baseline/DISRUPTION.vst3")),
       now = load(manager, juce::File(path));
  auto out = csv("phase7-audio-regression.csv");
  out << "rate,channels,mode,hq,setting,editor,maximum_difference,latency\n";
  for (double rate : rates)
    for (int channels : {1, 2})
      for (int mode = 0; mode < 5; ++mode)
        for (int hq : {0, 1})
          for (int setting = 0; setting < 3; ++setting) {
            layout(*old, channels);
            layout(*now, channels);
            float values[] = {mode / 4.f,
                              setting / 2.f,
                              setting / 2.f,
                              setting == 0   ? .5f
                              : setting == 1 ? .2f
                                             : .85f,
                              setting / 2.f,
                              setting / 2.f,
                              setting == 1 ? .4f : 1.f,
                              .5f,
                              hq ? 1.f : 0.f,
                              0};
            for (int i = 0; i < 10; ++i)
              old->getParameters()[i]->setValueNotifyingHost(values[i]);
            juce::MemoryBlock state;
            old->getStateInformation(state);
            now->setStateInformation(state.getData(),
                                     static_cast<int>(state.getSize()));
            for (int i = 0; i < 10; ++i)
              check(old->getParameters()[i]->getValue() ==
                        now->getParameters()[i]->getValue(),
                    "Phase8 archived state exact");
            old->prepareToPlay(rate, 128);
            now->prepareToPlay(rate, 128);
            std::unique_ptr<juce::AudioProcessorEditor> editor;
            if (setting == 1)
              editor.reset(now->createEditor());
            check(old->getLatencySamples() == 60 &&
                      now->getLatencySamples() == 60,
                  "Phase8 archived latency");
            juce::AudioBuffer<float> a(channels, 128), b(channels, 128);
            juce::MidiBuffer midi;
            for (int n = 0; n < 32; ++n) {
              fill(a, 9, n * 128, rate);
              b.makeCopyOf(a, true);
              old->processBlock(a, midi);
              now->processBlock(b, midi);
              for (int c = 0; c < channels; ++c)
                for (int i = 0; i < 128; ++i)
                  check(a.getSample(c, i) == b.getSample(c, i),
                        "Phase7 archived DLL exact audio match");
            }
            out << rate << ',' << channels << ',' << mode << ',' << hq << ','
                << setting << ',' << (editor ? "open" : "closed") << ",0,60\n";
            editor.reset();
            old->releaseResources();
            now->releaseResources();
          }
  std::cout << "Phase8 actual Phase7 DLL regression: 240 configurations; "
               "maximum difference 0\n";
}
void memoryRow(std::ofstream &out, int cycles) {
  PROCESS_MEMORY_COUNTERS_EX info{};
  info.cb = sizeof(info);
  check(GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&info),
                             sizeof(info)) != 0,
        "Phase8 memory query");
  DWORD handles = 0;
  check(GetProcessHandleCount(GetCurrentProcess(), &handles) != 0,
        "Phase8 handle query");
  out << cycles << ',' << info.PrivateUsage << ',' << info.WorkingSetSize << ','
      << handles << ',' << GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS)
      << '\n';
}
void lifecycle() {
  auto out = csv("editor-lifecycle-memory.csv");
  out << "cycles,private_bytes,working_set_bytes,handles,gdi_objects\n";
  juce::AudioBuffer<float> b(2, 128);
  std::unique_ptr<DisruptionProcessor> p;
  auto begin = Clock::now();
  for (int n = 0; n < 520; ++n) {
    if (n % 40 == 0) {
      p = std::make_unique<DisruptionProcessor>();
      p->presetManager().setUserDirectory(
          root().getChildFile("build/Phase8UserTests"));
      p->prepareToPlay(48000, 128);
    }
    {
      DisruptionEditor editor(*p);
      check(editor.getWidth() == 900 && editor.getHeight() == 600,
            "Phase8 fixed geometry");
      const auto &bank = p->presetManager().factoryPresets();
      check(p->presetManager()
                .load(bank[static_cast<size_t>(n) % bank.size()].id)
                .wasOk(),
            "Phase8 lifecycle preset");
      for (int step = 0; step < 3; ++step) {
        set(*p, 0, static_cast<float>((n + step) % 5));
        set(*p, 2, static_cast<float>((n * 19 + step * 31) % 101));
        set(*p, 8, static_cast<float>((n + step) % 2));
        set(*p, 9, static_cast<float>((n / 3 + step) % 2));
        fill(b, 9, n * 128, 48000);
        process(*p, b);
        finite(b);
        editor.refreshVisuals();
      }
      if (n % 20 == 0)
        pump();
    }
    if (n == 19 || (n + 1) % 50 == 0 || n == 519)
      memoryRow(out, n + 1);
  }
  p.reset();
  memoryRow(out, 521);
  std::cout << "Phase8 UI lifecycle: 520 editor cycles, 13 processor "
               "lifetimes; wall seconds "
            << std::chrono::duration<double>(Clock::now() - begin).count()
            << "\n";
}
void soak() {
  constexpr uint32_t seed = 0xD15A8E32u;
  uint32_t rng = seed;
  auto random = [&]() {
    rng = 1664525u * rng + 1013904223u;
    return static_cast<float>((rng >> 8) * (1.0 / 16777216.0));
  };
  auto out = csv("soak-segments.csv");
  out << "last_block,rate,channels,frames,total_audio_seconds,wall_seconds,"
         "peak,seed\n";
  DisruptionProcessor p;
  p.presetManager().setUserDirectory(
      root().getChildFile("build/Phase8UserTests"));
  const auto all = p.presetManager().factoryPresets();
  const float lo[] = {0, 0, 0, -100, 0, 0, 0, -24, 0, 0},
              hi[] = {4, 36, 100, 100, 100, 100, 100, 12, 1, 1};
  juce::AudioBuffer<float> b(2, 1024);
  int channels = 2;
  double rate = 48000, seconds = 0, peak = 0;
  int64_t frames = 0;
  auto begin = Clock::now();
  std::unique_ptr<DisruptionEditor> editor;
  for (int n = 0; n < 180000; ++n) {
    if (n % 5000 == 0) {
      editor.reset();
      rate = rates[(n / 5000) % 4];
      channels = (n / 5000) % 2 + 1;
      layout(p, channels);
      p.prepareToPlay(rate, 1024);
    }
    if (n % 113 == 0)
      check(p.presetManager()
                .load(all[static_cast<size_t>(n / 113) % all.size()].id)
                .wasOk(),
            "Phase8 soak preset");
    if (n % 64 == 0)
      for (int i = 0; i < 10; ++i) {
        float v = lo[i] + (hi[i] - lo[i]) * random();
        if (i == 0 || i >= 8)
          v = std::round(v);
        set(p, i, v);
      }
    if (n % 211 == 0) {
      juce::MemoryBlock s;
      p.getStateInformation(s);
      p.setStateInformation(s.getData(), static_cast<int>(s.getSize()));
    }
    if (n % 2500 == 0)
      editor = std::make_unique<DisruptionEditor>(p);
    if (n % 2500 == 32)
      editor.reset();
    const int size = sizes[n % 6];
    b.setSize(channels, size, false, false, true);
    fill(b, (n / 257) % 10, frames, rate);
    process(p, b);
    peak = std::max(peak, finite(b));
    frames += size;
    seconds += size / rate;
    if (editor && n % 8 == 0)
      editor->refreshVisuals();
    if ((n + 1) % 5000 == 0)
      out << n + 1 << ',' << rate << ',' << channels << ',' << frames << ','
          << seconds << ','
          << std::chrono::duration<double>(Clock::now() - begin).count() << ','
          << peak << ',' << seed << '\n';
  }
  std::cout << "Phase8 deterministic soak: 180000 blocks, " << frames
            << " frames, " << seconds << " simulated audio seconds; wall "
            << std::chrono::duration<double>(Clock::now() - begin).count()
            << " seconds; seed " << seed << "; peak " << peak << "\n";
}
void hosted(const char *path, bool clean = false) {
  auto begin = Clock::now();
  int instances = 0;
  for (int count : {1, 4, 8, 16}) {
    juce::AudioPluginFormatManager manager;
    manager.addFormat(std::make_unique<juce::VST3PluginFormat>());
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> plugins;
    for (int n = 0; n < count; ++n) {
      auto p = load(manager, juce::File(path));
      layout(*p, 2);
      p->getParameters()[0]->setValueNotifyingHost((n % 5) / 4.f);
      p->getParameters()[2]->setValueNotifyingHost(.85f);
      p->getParameters()[8]->setValueNotifyingHost((n % 2) ? 1.f : 0.f);
      p->prepareToPlay(48000, 128);
      check(p->getLatencySamples() == 60, "Phase8 hosted latency");
      plugins.push_back(std::move(p));
    }
    juce::AudioBuffer<float> b(2, 128);
    juce::MidiBuffer midi;
    for (auto &p : plugins) {
      for (int n = 0; n < 256; ++n) {
        fill(b, 9, n * 128, 48000);
        p->processBlock(b, midi);
        finite(b);
      }
      juce::MemoryBlock state;
      p->getStateInformation(state);
      std::array<float, 10> before{};
      for (int i = 0; i < 10; ++i)
        before[static_cast<size_t>(i)] = p->getParameters()[i]->getValue();
      p->releaseResources();
      p.reset();
      p = load(manager, juce::File(path));
      p->setStateInformation(state.getData(),
                             static_cast<int>(state.getSize()));
      for (int i = 0; i < 10; ++i)
        check(p->getParameters()[i]->getValue() ==
                  before[static_cast<size_t>(i)],
              "Phase8 hosted recreate state");
      p->prepareToPlay(48000, 128);
      {
        std::unique_ptr<juce::AudioProcessorEditor> e(p->createEditor());
        check(e != nullptr, "Phase8 hosted editor");
        e->addToDesktop(0);
        e->setVisible(true);
        pump();
        fill(b, 9, 0, 48000);
        p->processBlock(b, midi);
        finite(b);
        e->removeFromDesktop();
      }
      p->releaseResources();
      ++instances;
    }
  }
  std::cout
      << "Phase8 " << (clean ? "clean-location" : "hosted")
      << " 1/4/8/16 simultaneous instances; " << instances
      << " recreate/state/editor lifetimes; four format-manager reloads; wall "
      << std::chrono::duration<double>(Clock::now() - begin).count()
      << " seconds\n";
}
void performance() {
  auto out = csv("cpu.csv");
  out << "rate,mode,hq,editor,callbacks,median_us,p95_us,p99_us,median_percent_"
         "budget,p95_percent_budget,timer_fps,painted_frames\n";
  for (double rate : {48000., 96000.})
    for (int mode = 0; mode < 5; ++mode)
      for (int hq : {0, 1})
        for (int open : {0, 1}) {
          DisruptionProcessor p;
          p.presetManager().setUserDirectory(
              root().getChildFile("build/Phase8UserTests"));
          set(p, 0, static_cast<float>(mode));
          set(p, 1, 24);
          set(p, 2, 85);
          set(p, 3, 35);
          set(p, 4, 45);
          set(p, 5, 30);
          set(p, 8, static_cast<float>(hq));
          p.prepareToPlay(rate, 64);
          std::unique_ptr<DisruptionEditor> editor;
          if (open)
            editor = std::make_unique<DisruptionEditor>(p);
          juce::Image canvas(juce::Image::RGB, 900, 600, true);
          std::vector<double> times;
          times.reserve(20000);
          std::atomic<bool> done{false};
          auto begin = Clock::now();
          std::thread worker([&]() {
            juce::AudioBuffer<float> b(2, 64);
            juce::MidiBuffer midi;
            for (int n = 0; n < 20200; ++n) {
              fill(b, 9, n * 64, rate);
              auto start = Clock::now();
              track = true;
              p.processBlock(b, midi);
              track = false;
              if (n >= 200)
                times.push_back(std::chrono::duration<double, std::micro>(
                                    Clock::now() - start)
                                    .count());
            }
            done = true;
          });
          uint64_t last = 0;
          int painted = 0;
          while (!done.load() ||
                 (editor && Clock::now() - begin < std::chrono::seconds(1))) {
            pump();
            if (editor && editor->animationFrameCount() != last) {
              last = editor->animationFrameCount();
              juce::Graphics g(canvas);
              editor->paintEntireComponent(g, true);
              ++painted;
            }
          }
          worker.join();
          double wall =
              std::chrono::duration<double>(Clock::now() - begin).count();
          std::sort(times.begin(), times.end());
          out << rate << ',' << mode << ',' << hq << ','
              << (open ? "open" : "closed") << ",20000," << times[10000] << ','
              << times[19000] << ',' << times[19800] << ','
              << times[10000] * rate / 640000 << ','
              << times[19000] * rate / 640000 << ','
              << (editor ? editor->animationFrameCount() / wall : 0) << ','
              << painted << '\n';
          check(p.getLatencySamples() == 60, "Phase8 benchmark latency");
          if (editor)
            check(painted >= 20, "Phase8 UI timer benchmark");
        }
  std::cout << "Phase8 CPU: 40 configurations, 20000 timed callbacks each, "
               "actual 30 Hz timer and preallocated opaque paint canvas\n";
}
void similarity() {
  DisruptionProcessor p;
  const auto all = p.presetManager().factoryPresets();
  constexpr int frames = 48000;
  std::vector<std::vector<float>> waves;
  std::vector<juce::String> names;
  // One second of the same mixture of bass, vocal-like tones and transient
  // bursts for every preset.
  for (size_t n = 1; n < all.size(); ++n) {
    check(p.presetManager().load(all[n].id).wasOk(),
          "Phase8 similarity preset");
    layout(p, 1);
    p.prepareToPlay(48000, 128);
    juce::AudioBuffer<float> b(1, frames + 60);
    for (int i = 0; i < frames + 60; ++i) {
      double t = i / 48000.;
      double burst =
          std::exp(-std::fmod(t, .25) * 65) * std::sin(2 * pi * 3301 * t);
      b.setSample(0, i,
                  static_cast<float>(.12 * std::sin(2 * pi * 53 * t) +
                                     signal(9, i, 48000) + .09 * burst));
    }
    process(p, b);
    finite(b);
    std::vector<float> w(static_cast<size_t>(frames));
    double energy = 0;
    for (int i = 0; i < frames; ++i) {
      w[static_cast<size_t>(i)] = b.getSample(0, i + 60);
      energy += w[static_cast<size_t>(i)] * w[static_cast<size_t>(i)];
    }
    double rms = std::sqrt(energy / frames);
    check(rms > 0, "Phase8 similarity audible");
    for (auto &v : w)
      v = static_cast<float>(v / rms);
    waves.push_back(std::move(w));
    names.push_back(all[n].name);
  }
  auto out = csv("preset-similarity.csv");
  out << "preset_a,preset_b,normalized_rms_difference,correlation,flag\n";
  int pairs = 0, flags = 0;
  for (size_t a = 0; a < waves.size(); ++a)
    for (size_t b = a + 1; b < waves.size(); ++b) {
      double difference = 0, dot = 0;
      for (int i = 0; i < frames; ++i) {
        double x = waves[a][static_cast<size_t>(i)],
               y = waves[b][static_cast<size_t>(i)];
        difference += (x - y) * (x - y);
        dot += x * y;
      }
      double rms = std::sqrt(difference / frames);
      bool flag = rms < .10;
      out << names[a] << ',' << names[b] << ',' << rms << ',' << dot / frames
          << ',' << (flag ? "AUDITION" : "") << '\n';
      ++pairs;
      if (flag)
        ++flags;
    }
  std::cout
      << "Phase8 preset similarity: " << pairs << " common-source pairs, "
      << flags
      << " candidates below 0.10 normalized RMS difference; no retuning\n";
}
} // namespace
int runPhase8Clean(const char *path) {
  hosted(path, true);
  std::cout << checks << " clean-location checks passed\n";
  return checks;
}
int runPhase8Tests(const char *path) {
  auto begin = Clock::now();
  extreme();
  rateAndBuffer();
  archive(path);
  lifecycle();
  soak();
  hosted(path);
  performance();
  similarity();
  check(allocations == 0, "Phase8 callback allocations");
  std::cout << "Phase8: " << checks
            << " assertions passed; tracked callback allocations "
            << allocations << "; wall "
            << std::chrono::duration<double>(Clock::now() - begin).count()
            << " seconds\n";
  return checks;
}
