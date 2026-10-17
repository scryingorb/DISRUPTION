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
extern thread_local bool track;
extern int allocations;
namespace {
int checks = 0;
constexpr double pi = 3.141592653589793;
constexpr const char *ids[] = {"mode",  "drive", "damage", "tone", "body",
                               "crush", "mix",   "output", "hq",   "bypass"};
void check(bool v, const char *m) {
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
  auto f = root().getChildFile("Docs/Phase7");
  check(f.createDirectory().wasOk(), "Phase7 docs");
  return f;
}
void set(DisruptionProcessor &p, const char *id, float value) {
  auto *v = p.parameters.getParameter(id);
  v->setValueNotifyingHost(v->convertTo0to1(value));
}
juce::Component *find(juce::Component &parent, const juce::String &id) {
  if (parent.getComponentID() == id)
    return &parent;
  for (auto *child : parent.getChildren())
    if (auto *match = find(*child, id))
      return match;
  return nullptr;
}
void pump() {
  auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(8);
  do {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    Sleep(1);
  } while (std::chrono::steady_clock::now() < until);
}
void audio(DisruptionProcessor &p, juce::AudioBuffer<float> &b, int n = 0) {
  for (int c = 0; c < b.getNumChannels(); ++c)
    for (int i = 0; i < b.getNumSamples(); ++i)
      b.setSample(
          c, i,
          static_cast<float>(.18 * std::sin(2 * pi * 137 * (i + n) / 48000) +
                             .08 * std::sin(2 * pi * 551 * (i + n) / 48000)));
  juce::MidiBuffer midi;
  track = true;
  p.processBlock(b, midi);
  track = false;
}
void snapshot(DisruptionEditor &editor, const juce::String &name,
              float scale = 1) {
  editor.refreshVisuals();
  auto image =
      editor.createComponentSnapshot(editor.getLocalBounds(), true, scale);
  check(image.getWidth() == juce::roundToInt(900 * scale) &&
            image.getHeight() == juce::roundToInt(600 * scale),
        "scaled screenshot dimensions");
  auto file = docs().getChildFile("Screenshots/" + name + ".png");
  check(file.getParentDirectory().createDirectory().wasOk(), "screenshot dir");
  juce::FileOutputStream stream(file);
  check(stream.openedOk(), "screenshot stream");
  stream.setPosition(0);
  stream.truncate();
  juce::PNGImageFormat png;
  check(png.writeImageToStream(image, stream), "screenshot png");
}
void editorTests() {
  DisruptionProcessor p;
  auto directory = root().getChildFile("build/Phase7UserTests");
  p.presetManager().setUserDirectory(directory);
  p.prepareToPlay(48000, 128);
  DisruptionEditor editor(p);
  check(editor.getWidth() == 900 && editor.getHeight() == 600 &&
            !editor.isResizable(),
        "final editor dimensions fixed");
  check(p.visualizerBridge().isActive(), "bridge active with editor");
  const char *modes[] = {"BURN", "BREAK", "RIP", "CRUSH", "CHAOS"};
  auto all = p.presetManager().factoryPresets();
  auto *box = dynamic_cast<juce::ComboBox *>(find(editor, "presets"));
  check(box != nullptr && box->getNumItems() == 31,
        "factory browser inventory");
  for (auto &f : all) {
    check(p.presetManager().load(f.id).wasOk(), "UI preset load");
    pump();
    editor.refreshVisuals();
    check(box->getText() == f.name, "preset name fresh");
    for (int i = 1; i < 8; ++i) {
      auto *slider = dynamic_cast<juce::Slider *>(find(editor, ids[i]));
      check(slider != nullptr &&
                std::abs(slider->getValue() - f.values[i]) < .006,
            "slider attachment from presets");
      check(slider->getDoubleClickReturnValue() == (i == 6 ? 100 : 0),
            "default double click");
      check(!slider->getTooltip().isEmpty(), "knob tooltip");
      check(slider->getTextFromValue(slider->getValue())
                .contains(i == 1 || i == 7 ? "dB" : "%"),
            "displayed value units");
    }
    for (int i = 0; i < 5; ++i) {
      auto *button = dynamic_cast<juce::TextButton *>(
          find(editor, "mode-" + juce::String(modes[i])));
      check(button && button->getToggleState() == (i == f.values[0]),
            "mode selection synchronized");
    }
    for (int i : {8, 9}) {
      auto *toggle = dynamic_cast<juce::ToggleButton *>(find(editor, ids[i]));
      check(toggle && toggle->getToggleState() == (f.values[i] != 0),
            "HQ/bypass synchronized");
    }
  }
  for (int i = 1; i < 8; ++i) {
    auto *slider = dynamic_cast<juce::Slider *>(find(editor, ids[i]));
    slider->setValue(i == 3   ? -33
                     : i == 7 ? -3
                              : 42,
                     juce::sendNotificationSync);
    pump();
    check(std::abs(slider->getValue() -
                   p.parameters.getRawParameterValue(ids[i])->load()) < .006,
          "slider writes APVTS");
  }
  for (int i = 0; i < 5; ++i) {
    auto *button = dynamic_cast<juce::TextButton *>(
        find(editor, "mode-" + juce::String(modes[i])));
    button->onClick();
    check(p.parameters.getRawParameterValue("mode")->load() == i,
          "mode button automation gesture");
  }
  for (int i : {8, 9}) {
    auto *toggle = dynamic_cast<juce::ToggleButton *>(find(editor, ids[i]));
    toggle->setToggleState(true, juce::sendNotificationSync);
    check(p.parameters.getRawParameterValue(ids[i])->load() == 1,
          "toggle writes APVTS");
  }
  juce::AudioBuffer<float> b(2, 128);
  p.presetManager().load(all[0].id);
  pump();
  snapshot(editor, "01-INIT");
  int indexes[] = {1, 7, 13, 19, 25};
  for (int i = 0; i < 5; ++i) {
    p.presetManager().load(all[indexes[i]].id);
    for (int n = 0; n < 32; ++n)
      audio(p, b, n * 128);
    pump();
    editor.refreshVisuals();
    snapshot(editor, juce::String(i + 2).paddedLeft('0', 2) + "-" + modes[i]);
  }
  p.presetManager().load(all.back().id);
  set(p, "damage", 100);
  for (int n = 0; n < 50; ++n) {
    audio(p, b, n * 128);
    editor.refreshVisuals();
  }
  pump();
  snapshot(editor, "07-DAMAGE-100");
  snapshot(editor, "DAMAGE-100-2x", 2);
  for (float scale : {1.f, 1.25f, 1.5f, 2.f})
    snapshot(editor, "DPI-" + juce::String(juce::roundToInt(scale * 100)),
             scale);
  set(p, "bypass", 1);
  pump();
  snapshot(editor, "08-BYPASS");
  set(p, "bypass", 0);
  set(p, "hq", 1);
  pump();
  snapshot(editor, "09-HQ-ON");
  check(p.presetManager().saveUser("Fractured Signal").wasOk(),
        "UI user state save");
  set(p, "damage", 83);
  pump();
  editor.refreshVisuals();
  check(box->getText() == "Fractured Signal *", "modified user display");
  snapshot(editor, "10-USER-MODIFIED");
  juce::MemoryBlock state;
  p.getStateInformation(state);
  p.presetManager().load(all[0].id);
  p.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
  pump();
  editor.refreshVisuals();
  check(box->getText() == "Fractured Signal *", "UI restored project identity");
  for (int i = 1; i < 8; ++i) {
    auto *slider = dynamic_cast<juce::Slider *>(find(editor, ids[i]));
    check(std::abs(slider->getValue() -
                   p.parameters.getRawParameterValue(ids[i])->load()) < .006,
          "UI project values");
  }
  check(p.presetManager().deleteUser(p.presetManager().currentId()).wasOk(),
        "remove isolated user test");
}
void lifecycle() {
  DisruptionProcessor p;
  p.presetManager().setUserDirectory(
      root().getChildFile("build/Phase7UserTests"));
  p.prepareToPlay(48000, 64);
  juce::AudioBuffer<float> b(2, 64);
  for (int i = 0; i < 40; ++i) {
    auto editor = std::make_unique<DisruptionEditor>(p);
    set(p, "mode", i % 5);
    set(p, "damage", i % 2 ? 100 : 0);
    for (int k = 0; k < 8; ++k)
      audio(p, b, (i * 8 + k) * 64);
    pump();
    editor->refreshVisuals();
    editor.reset();
    check(!p.visualizerBridge().isActive(), "editor close disables bridge");
    audio(p, b, i * 512);
  }
  check(allocations == 0, "UI playback callback allocations");
  // Editor creation/destruction and painting while the audio thread runs.
  std::atomic<bool> stop{false}, finite{true};
  std::thread thread([&] {
    juce::AudioBuffer<float> block(2, 64);
    int n = 0;
    while (!stop) {
      audio(p, block, n++ * 64);
      for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 64; ++i)
          if (!std::isfinite(block.getSample(c, i)))
            finite = false;
    }
  });
  for (int i = 0; i < 10; ++i) {
    auto editor = std::make_unique<DisruptionEditor>(p);
    pump();
    editor->refreshVisuals();
    auto image = editor->createComponentSnapshot(editor->getLocalBounds());
    check(image.isValid(), "live audio editor paint");
    editor.reset();
  }
  stop = true;
  thread.join();
  check(finite.load() && allocations == 0,
        "concurrent editor lifecycle stable");
}
void memoryAndVisualizer() {
  std::ofstream csv(
      docs().getChildFile("ui-memory.csv").getFullPathName().toStdString());
  csv << "item,bytes,interpretation\n";
  csv << "processor_bridge," << sizeof(disruption::VisualizerBridge)
      << ",actual sizeof including FIFO and 1024 floats\n";
  csv << "visualizer_object," << sizeof(disruption::ui::DisruptionVisualizer)
      << ",actual sizeof including Component and preallocated wave and "
         "particles\n";
  csv << "particle_pool," << 24 * 5 * sizeof(float)
      << ",24 particles with five floats each\n";
  csv << "knob_cache_100_percent," << (6 * 155 * 115 + 282 * 308) * 4
      << ",upper bound ARGB cache pixel storage excluding image overhead\n";
  csv << "knob_cache_200_percent," << (6 * 155 * 115 + 282 * 308) * 4 * 4
      << ",upper bound ARGB cache pixel storage at 2x\n";
  DisruptionProcessor p;
  p.prepareToPlay(48000, 128);
  disruption::ui::DisruptionVisualizer view(p.visualizerBridge());
  view.setBounds(0, 0, 846, 370);
  juce::AudioBuffer<float> b(2, 128);
  b.clear();
  b.setSample(0, 0, .5f);
  b.setSample(1, 0, -.5f);
  track = true;
  p.visualizerBridge().push(b);
  track = false;
  view.update(100, 4, false);
  check(view.currentPeak() > .49f, "anti-phase stereo remains audio-reactive");
  std::vector<double> times;
  times.reserve(100);
  std::vector<double> updates, paints;
  updates.reserve(100);
  paints.reserve(100);
  juce::Image canvas(juce::Image::ARGB, 846, 370, true);
  for (int n = 0; n < 100; ++n) {
    audio(p, b, n * 128);
    auto begin = std::chrono::steady_clock::now();
    view.update(100, 4, false);
    updates.push_back(std::chrono::duration<double, std::micro>(
                          std::chrono::steady_clock::now() - begin)
                          .count());
    juce::Graphics graphics(canvas);
    graphics.fillAll(juce::Colour(0xff101215));
    auto paintStart = std::chrono::steady_clock::now();
    view.paint(graphics);
    paints.push_back(std::chrono::duration<double, std::micro>(
                         std::chrono::steady_clock::now() - paintStart)
                         .count());
    times.push_back(std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - begin)
                        .count());
  }
  std::sort(updates.begin(), updates.end());
  std::sort(paints.begin(), paints.end());
  std::sort(times.begin(), times.end());
  std::ofstream timing(docs()
                           .getChildFile("visualizer-performance.csv")
                           .getFullPathName()
                           .toStdString());
  timing << "case,median_us,p95_us\nupdate_clear_and_wave_particles_max_damage,"
         << times[50] << "," << times[95] << "\n";
  timing << "ui_update," << updates[50] << "," << updates[95] << "\n";
  timing << "direct_paint_on_opaque_background," << paints[50] << ","
         << paints[95] << "\n";
}
void performance() {
  std::ofstream csv(docs()
                        .getChildFile("ui-performance.csv")
                        .getFullPathName()
                        .toStdString());
  csv << "mode,hq,editor,median_us,p95_us,frame_median_us,frame_p95_us,frames,"
         "measured_fps\n";
  for (int visualMode : {0, 4})
    for (bool hq : {false, true})
      for (bool open : {false, true}) {
        DisruptionProcessor p;
        p.presetManager().setUserDirectory(
            root().getChildFile("build/Phase7UserTests"));
        set(p, "mode", static_cast<float>(visualMode));
        set(p, "drive", 18);
        set(p, "damage", 95);
        set(p, "tone", 25);
        set(p, "body", 60);
        set(p, "crush", 30);
        set(p, "hq", hq ? 1 : 0);
        p.prepareToPlay(48000, 64);
        std::unique_ptr<DisruptionEditor> editor;
        if (open)
          editor = std::make_unique<DisruptionEditor>(p);
        std::vector<double> callbacks, frames;
        callbacks.reserve(10000);
        frames.reserve(120);
        std::atomic<bool> done{false};
        auto begin = std::chrono::steady_clock::now();
        std::thread worker([&] {
          juce::AudioBuffer<float> b(2, 64);
          juce::MidiBuffer midi;
          for (int n = 0; n < 10200; ++n) {
            for (int c = 0; c < 2; ++c)
              for (int i = 0; i < 64; ++i)
                b.setSample(
                    c, i,
                    static_cast<float>(
                        .2 * std::sin(2 * pi * 173 * (n * 64 + i) / 48000)));
            auto start = std::chrono::steady_clock::now();
            track = true;
            p.processBlock(b, midi);
            track = false;
            double us = std::chrono::duration<double, std::micro>(
                            std::chrono::steady_clock::now() - start)
                            .count();
            if (n >= 200)
              callbacks.push_back(us);
          }
          done = true;
        });
        uint64_t lastFrame = 0;
        while (!done.load() ||
               (editor && std::chrono::steady_clock::now() - begin <
                              std::chrono::seconds(1))) {
          pump();
          if (editor && editor->animationFrameCount() != lastFrame) {
            lastFrame = editor->animationFrameCount();
            auto start = std::chrono::steady_clock::now();
            auto image =
                editor->createComponentSnapshot(editor->getLocalBounds());
            frames.push_back(std::chrono::duration<double, std::micro>(
                                 std::chrono::steady_clock::now() - start)
                                 .count());
          }
        }
        worker.join();
        double seconds = std::chrono::duration<double>(
                             std::chrono::steady_clock::now() - begin)
                             .count();
        std::sort(callbacks.begin(), callbacks.end());
        std::sort(frames.begin(), frames.end());
        csv << visualMode << "," << hq << "," << (open ? "open" : "closed")
            << "," << callbacks[5000] << "," << callbacks[9500] << ","
            << (frames.empty() ? 0 : frames[frames.size() / 2]) << ","
            << (frames.empty() ? 0 : frames[frames.size() * 95 / 100]) << ","
            << frames.size() << ","
            << (editor ? editor->animationFrameCount() / seconds : 0) << "\n";
        check(p.getLatencySamples() == 60 && allocations == 0,
              "UI performance latency allocations");
        if (editor)
          check(editor->animationFrameCount() >= 20, "animated timer cadence");
      }
}
void archive(const char *path) {
  juce::AudioPluginFormatManager manager;
  manager.addFormat(std::make_unique<juce::VST3PluginFormat>());
  auto load = [&](juce::File file) {
    juce::OwnedArray<juce::PluginDescription> descriptions;
    manager.getFormat(0)->findAllTypesForFile(descriptions,
                                              file.getFullPathName());
    check(descriptions.size() == 1, "Phase7 hosted discovery");
    juce::String error;
    auto p = manager.createPluginInstance(*descriptions[0], 48000, 128, error);
    check(p != nullptr, "Phase7 hosted load");
    return p;
  };
  auto old = load(root().getChildFile("build/Phase6Baseline/DISRUPTION.vst3")),
       current = load(juce::File(path));
  std::ofstream csv(docs()
                        .getChildFile("phase6-audio-regression.csv")
                        .getFullPathName()
                        .toStdString());
  csv << "rate,channels,mode,hq,setting,editor,maximum_difference,latency\n";
  double worst = 0;
  for (double rate : {44100., 48000., 88200., 96000.})
    for (int ch : {1, 2})
      for (int mode = 0; mode < 5; ++mode)
        for (int hq : {0, 1})
          for (int setting = 0; setting < 3; ++setting) {
            for (auto *p : {old.get(), current.get()}) {
              juce::AudioProcessor::BusesLayout layout;
              auto s = ch == 1 ? juce::AudioChannelSet::mono()
                               : juce::AudioChannelSet::stereo();
              layout.inputBuses.add(s);
              layout.outputBuses.add(s);
              check(p->setBusesLayout(layout), "Phase7 archive layout");
              float v[] = {mode / 4.f,
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
                p->getParameters()[i]->setValueNotifyingHost(v[i]);
            }
            juce::MemoryBlock state;
            old->getStateInformation(state);
            current->setStateInformation(state.getData(),
                                         static_cast<int>(state.getSize()));
            for (int i = 0; i < 10; ++i)
              check(old->getParameters()[i]->getValue() ==
                        current->getParameters()[i]->getValue(),
                    "Phase6 state compatibility");
            for (auto *p : {old.get(), current.get()})
              p->prepareToPlay(rate, 128);
            std::unique_ptr<juce::AudioProcessorEditor> editor;
            if (setting == 1)
              editor.reset(current->createEditor());
            check(old->getLatencySamples() == 60 &&
                      current->getLatencySamples() == 60,
                  "Phase7 fixed latency");
            juce::AudioBuffer<float> a(ch, 128), b(ch, 128);
            juce::MidiBuffer midi;
            double maximum = 0;
            for (int n = 0; n < 32; ++n) {
              for (int c = 0; c < ch; ++c)
                for (int i = 0; i < 128; ++i)
                  a.setSample(
                      c, i,
                      static_cast<float>(
                          .22 * std::sin(2 * pi * 137 * (n * 128 + i) / rate) +
                          .1 * std::sin(2 * pi * 1551 * (n * 128 + i) / rate)) *
                          (c ? -1 : 1));
              b.makeCopyOf(a, true);
              old->processBlock(a, midi);
              current->processBlock(b, midi);
              for (int c = 0; c < ch; ++c)
                for (int i = 0; i < 128; ++i) {
                  double diff = std::abs(a.getSample(c, i) - b.getSample(c, i));
                  maximum = std::max(maximum, diff);
                  check(diff == 0, "Phase6 exact audio match");
                }
            }
            worst = std::max(worst, maximum);
            csv << rate << "," << ch << "," << mode << "," << hq << ","
                << setting << "," << (editor ? "open" : "closed") << ","
                << maximum << ",60\n";
            editor.reset();
            for (auto *p : {old.get(), current.get()})
              p->releaseResources();
          }
  std::cout << "Phase6 archived DLL audio match maximum difference " << worst
            << "\n";
}
} // namespace
int runPhase7Profile() {
  memoryAndVisualizer();
  return checks;
}
int runPhase7Tests(const char *path) {
  editorTests();
  lifecycle();
  performance();
  memoryAndVisualizer();
  archive(path);
  check(allocations == 0, "Phase7 callback allocations");
  std::cout << "Phase7: " << checks
            << " assertions passed; callback allocations " << allocations
            << "\n";
  return checks;
}
