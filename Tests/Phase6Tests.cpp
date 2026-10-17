#include "PluginProcessor.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>
extern thread_local bool track;
extern int allocations;
namespace {
int count = 0;
constexpr double pi = 3.141592653589793;
constexpr const char *ids[] = {"mode",  "drive", "damage", "tone", "body",
                               "crush", "mix",   "output", "hq",   "bypass"};
void check(bool v, const char *m) {
  ++count;
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
  auto f = root().getChildFile("Docs/Phase6");
  check(f.createDirectory().wasOk(), "Phase6 docs");
  return f;
}
void set(DisruptionProcessor &p, int i, float v) {
  auto *param = p.parameters.getParameter(ids[i]);
  param->setValueNotifyingHost(param->convertTo0to1(v));
}
void layout(juce::AudioProcessor &p, int ch) {
  juce::AudioProcessor::BusesLayout l;
  auto s =
      ch == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
  l.inputBuses.add(s);
  l.outputBuses.add(s);
  check(p.setBusesLayout(l), "Phase6 layout");
}
void process(DisruptionProcessor &p, juce::AudioBuffer<float> &b) {
  juce::MidiBuffer midi;
  track = true;
  p.processBlock(b, midi);
  track = false;
}
void bankAndFiles() {
  DisruptionProcessor p;
  auto &m = p.presetManager();
  auto all = m.factoryPresets();
  check(all.size() == 31, "INIT plus30");
  std::set<juce::String> names, identity;
  int families[5] = {};
  for (auto &f : all) {
    check(names.insert(f.name.toLowerCase()).second, "unique factory name");
    check(identity.insert(f.id).second, "unique factory id");
    disruption::Preset parsed;
    auto xml = disruption::PresetManager::serialize(f);
    check(disruption::PresetManager::parse(*xml, parsed).wasOk(),
          "factory parses");
    check(parsed.values == f.values, "factory roundtrip");
    check(f.values[9] == 0, "factory bypass off");
    check(m.load(f.id).wasOk() && !m.isModified(), "factory clean load");
    for (int i = 0; i < 10; ++i) {
      auto *param = p.parameters.getParameter(ids[i]);
      check(std::abs(param->convertFrom0to1(param->getValue()) - f.values[i]) <
                .006,
            "factory ranges and values");
    }
    if (f.category != "INIT") {
      auto mode = static_cast<int>(f.values[0]);
      check(mode >= 0 && mode < 5, "factory enum");
      check(f.category == juce::StringArray{"BURN", "BREAK", "RIP", "CRUSH",
                                            "CHAOS"}[mode],
            "category matches mode");
      ++families[mode];
    }
  }
  for (auto n : families)
    check(n == 6, "six per family");
  check(all[0].values == disruption::Preset{}.values, "safe INIT");
  m.load(all[0].id);
  check(m.previous().wasOk() && m.currentId() == all.back().id,
        "previous wrap");
  check(m.next().wasOk() && m.currentId() == all[0].id, "next wrap");
  for (size_t i = 1; i < all.size(); ++i)
    check(m.next().wasOk() && m.currentId() == all[i].id, "ordered next");
  m.load(all[1].id);
  set(p, 2, 25);
  check(m.isModified(), "dirty change");
  set(p, 2, all[1].values[2]);
  check(!m.isModified(), "dirty revert");
  auto directory = root().getChildFile("build/Phase6UserTests");
  check(directory.createDirectory().wasOk(), "test dir");
  for (auto file : directory.findChildFiles(juce::File::findFiles, false,
                                            "*.disruptionpreset"))
    check(file.deleteFile(), "cleanup isolated test files");
  m.setUserDirectory(directory);
  set(p, 9, 1);
  check(m.saveUser("Zeta Test").wasOk(), "user save");
  auto user = m.entries().back();
  auto bytes = user.file.loadFileAsString();
  set(p, 2, 99);
  check(m.saveUser("zeta test").failed(), "overwrite denied casefold");
  check(user.file.loadFileAsString() == bytes, "denied overwrite intact");
  check(m.saveUser("Zeta Test", true).wasOk(), "explicit overwrite");
  m.load(all[0].id);
  check(m.loadFile(user.file).wasOk(), "user file load");
  check(p.parameters.getRawParameterValue("damage")->load() == 99 &&
            p.parameters.getRawParameterValue("bypass")->load() == 1,
        "user all parameters");
  check(m.saveUser("Alpha Test").wasOk(), "second user");
  check(m.entries()[31].name == "Alpha Test", "user sorted");
  check(m.saveUser("../unsafe").failed(), "name path protection");
  check(m.deleteUser(all[0].id).failed(), "factory delete blocked");
  auto xml = disruption::PresetManager::serialize(all[1]);
  xml->getChildByName("PARAM")->setAttribute("value", "NaN");
  disruption::Preset out;
  check(disruption::PresetManager::parse(*xml, out).failed(), "NaN rejected");
  for (auto text : {"inf", "1junk", "0.5", "99", ""}) {
    xml->getChildByName("PARAM")->setAttribute("value", text);
    check(disruption::PresetManager::parse(*xml, out).failed(),
          "invalid enum rejected");
  }
  xml = disruption::PresetManager::serialize(all[1]);
  xml->setAttribute("version", 2);
  check(disruption::PresetManager::parse(*xml, out).failed(),
        "future version rejected");
  xml->setAttribute("version", 1);
  xml->getChildElement(1)->setAttribute("value", 999);
  check(disruption::PresetManager::parse(*xml, out).wasOk() &&
            out.values[1] == 36,
        "continuous clamp");
  xml->addChildElement(new juce::XmlElement(*xml->getChildElement(1)));
  check(disruption::PresetManager::parse(*xml, out).failed(),
        "duplicate rejected");
  xml = disruption::PresetManager::serialize(all[1]);
  xml->deleteAllChildElements();
  xml->createNewChildElement("FUTURE_METADATA");
  auto *unknown = xml->createNewChildElement("PARAM");
  unknown->setAttribute("id", "future");
  unknown->setAttribute("value", "NaN");
  check(disruption::PresetManager::parse(*xml, out).wasOk() &&
            out.values == disruption::Preset{}.values,
        "missing defaults unknown ignored");
  auto corrupt = directory.getChildFile("corrupt.disruptionpreset");
  check(corrupt.replaceWithText("broken XML"), "corrupt fixture");
  float before = p.parameters.getRawParameterValue("damage")->load();
  auto name = m.currentName();
  check(m.loadFile(corrupt).failed() &&
            p.parameters.getRawParameterValue("damage")->load() == before &&
            m.currentName() == name,
        "failed load preserves state");
  check(m.refreshUsers().size() == 1, "corrupt skipped");
  m.loadFile(user.file);
  set(p, 1, 17);
  juce::MemoryBlock state;
  p.getStateInformation(state);
  check(user.file.deleteFile(), "remove external preset");
  DisruptionProcessor restored;
  restored.presetManager().setUserDirectory(directory);
  restored.setStateInformation(state.getData(),
                               static_cast<int>(state.getSize()));
  check(restored.presetManager().currentName() == "Zeta Test" &&
            restored.presetManager().isModified(),
        "project name dirty independent of file");
  for (int i = 0; i < 10; ++i)
    check(p.getParameters()[i]->getValue() ==
              restored.getParameters()[i]->getValue(),
          "exact project params");
  m.refreshUsers();
  check(m.deleteUser(m.entries().back().id).wasOk(), "delete user");
  check(corrupt.deleteFile(), "cleanup corruption");
}
float source(const juce::String &type, int n, double rate) {
  double t = n / rate;
  double a = std::sin(2 * pi * 173 * t), b = std::sin(2 * pi * 519 * t),
         c = std::sin(2 * pi * 1211 * t);
  if (type == "bass")
    return static_cast<float>(.22 * std::sin(2 * pi * 55 * t) +
                              .07 * std::sin(2 * pi * 110 * t));
  if (type == "drums") {
    double beat = std::fmod(t, .5);
    double noise = std::sin(n * 12.9898) * 43758.5453;
    noise = 2 * (noise - std::floor(noise)) - 1;
    return static_cast<float>(
        .45 * std::exp(-beat * 24) *
            std::sin(2 * pi * (55 * t + 3 * std::exp(-beat * 20))) +
        .14 * std::exp(-std::fmod(t + .25, .5) * 40) * noise);
  }
  if (type == "vocal")
    return static_cast<float>((.15 * a + .06 * b + .03 * c) *
                              (.65 + .35 * std::sin(2 * pi * 3 * t)));
  if (type == "guitar")
    return static_cast<float>(
        (.13 * a + .07 * b + .04 * c + .02 * std::sin(2 * pi * 2422 * t)) *
        (.55 + .45 * std::exp(-std::fmod(t, .7) * 5)));
  if (type == "fx")
    return static_cast<float>(.15 * std::sin(2 * pi * (90 * t + 900 * t * t)) +
                              .08 * std::sin(2 * pi * 33 * t));
  return static_cast<float>(.12 * a + .08 * std::sin(2 * pi * 277 * t) +
                            .05 * std::sin(2 * pi * 997 * t));
}
void playback() {
  std::ofstream csv(
      docs().getChildFile("switching.csv").getFullPathName().toStdString());
  csv << "rate,channels,switches,peak,max_step,latency\n";
  for (double rate : {44100., 48000., 88200., 96000.})
    for (int ch : {1, 2}) {
      DisruptionProcessor p;
      layout(p, ch);
      p.prepareToPlay(rate, 64);
      juce::AudioBuffer<float> b(ch, 64);
      double peak = 0, step = 0;
      float previous[2] = {};
      auto all = p.presetManager().factoryPresets();
      for (int k = 0; k < 620; ++k) {
        check(p.presetManager().load(all[(k * 13) % 31].id).wasOk(),
              "rapid load");
        for (int c = 0; c < ch; ++c)
          for (int i = 0; i < 64; ++i)
            b.setSample(c, i, source("synth", k * 64 + i, rate) * (c ? -1 : 1));
        process(p, b);
        for (int c = 0; c < ch; ++c)
          for (int i = 0; i < 64; ++i) {
            double v = b.getSample(c, i);
            check(std::isfinite(v), "switch finite");
            peak = std::max(peak, std::abs(v));
            step = std::max(step, std::abs(v - previous[c]));
            previous[c] = static_cast<float>(v);
          }
        check(p.getLatencySamples() == 60, "switch fixed latency");
      }
      check(peak < 4, "switch bounded peak");
      csv << rate << "," << ch << ",620," << peak << "," << step << ",60\n";
    }
  // An incomplete parameter batch must continue with the preceding stable
  // controls.
  DisruptionProcessor p, ref;
  layout(p, 1);
  layout(ref, 1);
  p.prepareToPlay(48000, 64);
  ref.prepareToPlay(48000, 64);
  p.presetManager().revision.fetch_add(1);
  set(p, 1, 36);
  set(p, 2, 100);
  juce::AudioBuffer<float> a(1, 64), b(1, 64);
  for (int k = 0; k < 8; ++k) {
    for (int i = 0; i < 64; ++i)
      a.setSample(0, i, source("vocal", k * 64 + i, 48000));
    b.makeCopyOf(a, true);
    process(p, a);
    process(ref, b);
    for (int i = 0; i < 64; ++i)
      check(a.getSample(0, i) == b.getSample(0, i), "partial batch withheld");
  }
  p.presetManager().revision.fetch_add(1);
  DisruptionProcessor concurrent;
  layout(concurrent, 2);
  concurrent.prepareToPlay(48000, 64);
  std::atomic<bool> start{false}, finished{false}, loadsOk{true};
  std::thread writer([&] {
    while (!start.load())
      std::this_thread::yield();
    auto presets = concurrent.presetManager().factoryPresets();
    for (int k = 0; k < 500; ++k)
      if (concurrent.presetManager().load(presets[k % 31].id).failed())
        loadsOk = false;
    finished = true;
  });
  juce::AudioBuffer<float> block(2, 64);
  start = true;
  int blocks = 0;
  while (!finished.load() || blocks < 500) {
    for (int c = 0; c < 2; ++c)
      for (int i = 0; i < 64; ++i)
        block.setSample(c, i, source("vocal", blocks * 64 + i, 48000));
    process(concurrent, block);
    for (int c = 0; c < 2; ++c)
      for (int i = 0; i < 64; ++i)
        check(std::isfinite(block.getSample(c, i)),
              "concurrent switching finite");
    ++blocks;
  }
  writer.join();
  check(loadsOk.load(), "concurrent loads successful");
  check(allocations == 0, "Phase6 callback allocations");
}
void renders() {
  auto folder = docs().getChildFile("Listening");
  check(folder.createDirectory().wasOk(), "preset render dir");
  juce::WavAudioFormat wav;
  const int frames = 192000;
  juce::AudioBuffer<float> sequence(2, frames * 31);
  sequence.clear();
  std::ofstream csv(
      docs().getChildFile("preset-levels.csv").getFullPathName().toStdString());
  csv << "category,name,source,input_rms,rms,peak,gain_db,low_energy_"
         "fraction\n";
  auto write = [&](juce::File file, juce::AudioBuffer<float> &b) {
    auto stream = std::make_unique<juce::FileOutputStream>(file);
    check(stream->openedOk(), "preset WAV stream");
    stream->setPosition(0);
    stream->truncate();
    std::unique_ptr<juce::OutputStream> out = std::move(stream);
    auto writer = wav.createWriterFor(
        out,
        juce::AudioFormatWriterOptions{}
            .withSampleRate(48000)
            .withNumChannels(2)
            .withBitsPerSample(32)
            .withSampleFormat(
                juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
    check(writer && writer->writeFromAudioSampleBuffer(b, 0, b.getNumSamples()),
          "preset WAV write");
  };
  DisruptionProcessor p;
  auto all = p.presetManager().factoryPresets();
  std::vector<std::vector<float>> common;
  std::set<juce::String> writtenSources;
  for (size_t index = 0; index < all.size(); ++index) {
    auto &preset = all[index];
    p.presetManager().load(preset.id);
    p.prepareToPlay(48000, 256);
    juce::AudioBuffer<float> audio(2, frames);
    double input = 0;
    for (int c = 0; c < 2; ++c)
      for (int i = 0; i < frames; ++i) {
        float v = source(preset.source, i, 48000) * (c ? .93f : 1.f);
        audio.setSample(c, i, v);
        input += v * v;
      }
    if (writtenSources.insert(preset.source).second)
      write(folder.getChildFile("Source-" + preset.source + ".wav"), audio);
    process(p, audio);
    double energy = 0, low = 0;
    float pole = static_cast<float>(std::exp(-2 * pi * 350 / 48000));
    float lp[2] = {};
    for (int c = 0; c < 2; ++c)
      for (int i = 0; i < frames; ++i) {
        float v = audio.getSample(c, i);
        check(std::isfinite(v), "render finite");
        energy += v * v;
        lp[c] = (1 - pole) * v + pole * lp[c];
        low += lp[c] * lp[c];
      }
    double rms = std::sqrt(energy / (frames * 2)),
           in = std::sqrt(input / (frames * 2));
    double peak = audio.getMagnitude(0, frames);
    csv << preset.category << "," << preset.name << "," << preset.source << ","
        << in << "," << rms << "," << peak << "," << 20 * std::log10(rms / in)
        << "," << low / energy << "\n";
    check(rms > 1e-5 && peak < 1, "render audible bounded");
    audio.applyGainRamp(0, 480, 0, 1);
    audio.applyGainRamp(frames - 480, 480, 1, 0);
    auto stem = preset.category + "-" + preset.name.replaceCharacter(' ', '-');
    write(folder.getChildFile(stem + "-Raw.wav"), audio);
    audio.applyGain(static_cast<float>(.1 / rms));
    write(folder.getChildFile(stem + "-Matched.wav"), audio);
    for (int c = 0; c < 2; ++c)
      sequence.copyFrom(c, static_cast<int>(index) * frames, audio, c, 0,
                        frames);
    p.prepareToPlay(48000, 256);
    juce::AudioBuffer<float> reference(1, 24000);
    layout(p, 1);
    p.prepareToPlay(48000, 256);
    for (int i = 0; i < 24000; ++i)
      reference.setSample(0, i, source("synth", i, 48000));
    process(p, reference);
    std::vector<float> normalized(20000);
    double sum = 0;
    for (int i = 0; i < 20000; ++i) {
      normalized[i] = reference.getSample(0, i + 4000);
      sum += normalized[i] * normalized[i];
    }
    float gain = static_cast<float>(std::sqrt(20000 / sum));
    for (auto &v : normalized)
      v *= gain;
    common.push_back(std::move(normalized));
    layout(p, 2);
  }
  write(folder.getChildFile("INIT-and-30-Matched-Sequence.wav"), sequence);
  std::ofstream distinct(
      docs().getChildFile("distinctness.csv").getFullPathName().toStdString());
  distinct << "category,a,b,normalized_difference_rms\n";
  bool allDistinct = true;
  for (int family = 0; family < 5; ++family)
    for (int a = 0; a < 6; ++a)
      for (int b = a + 1; b < 6; ++b) {
        int x = 1 + family * 6 + a, y = 1 + family * 6 + b;
        double sum = 0;
        for (int i = 0; i < 20000; ++i)
          sum += std::pow(common[x][i] - common[y][i], 2);
        double diff = std::sqrt(sum / 20000);
        distinct << all[x].category << "," << all[x].name << "," << all[y].name
                 << "," << diff << "\n";
        if (diff <= .05) {
          allDistinct = false;
          std::cout << "Near pair: " << all[x].name << " / " << all[y].name
                    << " diff " << diff << "\n";
        }
      }
  check(allDistinct, "near duplicate factory audio");
}
void archived(const char *path) {
  juce::AudioPluginFormatManager manager;
  manager.addFormat(std::make_unique<juce::VST3PluginFormat>());
  auto load = [&](juce::File file) {
    juce::OwnedArray<juce::PluginDescription> descriptions;
    manager.getFormat(0)->findAllTypesForFile(descriptions,
                                              file.getFullPathName());
    check(descriptions.size() == 1, "Phase6 hosted discovery");
    juce::String error;
    auto p = manager.createPluginInstance(*descriptions[0], 48000, 64, error);
    check(p != nullptr, "Phase6 hosted load");
    return p;
  };
  auto old = load(root().getChildFile("build/Phase5Baseline/DISRUPTION.vst3")),
       now = load(juce::File(path));
  std::ofstream cpu(
      docs().getChildFile("phase5-cpu.csv").getFullPathName().toStdString());
  cpu << "rate,mode,hq,version,median_us,p95_us\n";
  double worst = 0;
  for (double rate : {44100., 48000., 88200., 96000.})
    for (int ch : {1, 2})
      for (int mode = 0; mode < 5; ++mode)
        for (int hq : {0, 1}) {
          for (auto *p : {old.get(), now.get()}) {
            layout(*p, ch);
            float normalized[] = {
                mode / 4.f, .5f,  .75f,      .35f,           .6f,
                .45f,       .65f, 18.f / 36, hq ? 1.f : 0.f, 0};
            for (int i = 0; i < 10; ++i)
              p->getParameters()[i]->setValueNotifyingHost(normalized[i]);
          }
          juce::MemoryBlock state;
          old->getStateInformation(state);
          now->setStateInformation(state.getData(),
                                   static_cast<int>(state.getSize()));
          for (int i = 0; i < 10; ++i)
            check(old->getParameters()[i]->getValue() ==
                      now->getParameters()[i]->getValue(),
                  "Phase5 state compatibility");
          for (auto *p : {old.get(), now.get()})
            p->prepareToPlay(rate, 64);
          check(old->getLatencySamples() == 60 &&
                    now->getLatencySamples() == 60,
                "archive latency unchanged");
          juce::AudioBuffer<float> a(ch, 64), b(ch, 64);
          juce::MidiBuffer midi;
          for (int k = 0; k < 24; ++k) {
            for (int c = 0; c < ch; ++c)
              for (int i = 0; i < 64; ++i)
                a.setSample(c, i,
                            source("synth", k * 64 + i, rate) * (c ? -1 : 1));
            b.makeCopyOf(a, true);
            old->processBlock(a, midi);
            now->processBlock(b, midi);
            for (int c = 0; c < ch; ++c)
              for (int i = 0; i < 64; ++i) {
                worst = std::max(worst,
                                 static_cast<double>(std::abs(
                                     a.getSample(c, i) - b.getSample(c, i))));
                check(a.getSample(c, i) == b.getSample(c, i),
                      "Phase5 exact DSP regression");
              }
          }
          now->getStateInformation(state);
          old->setStateInformation(state.getData(),
                                   static_cast<int>(state.getSize()));
          for (int i = 0; i < 10; ++i)
            check(old->getParameters()[i]->getValue() ==
                      now->getParameters()[i]->getValue(),
                  "Phase5 reads new metadata");
          for (auto *p : {old.get(), now.get()})
            p->releaseResources();
        }
  for (double rate : {48000., 96000.})
    for (int mode = 0; mode < 5; ++mode)
      for (int hq : {0, 1})
        for (int version : {0, 1}) {
          auto *p = version == 0 ? old.get() : now.get();
          layout(*p, 2);
          float v[] = {mode / 4.f, .5f,  .75f, .35f,           .6f,
                       .45f,       .65f, .5f,  hq ? 1.f : 0.f, 0};
          for (int i = 0; i < 10; ++i)
            p->getParameters()[i]->setValueNotifyingHost(v[i]);
          p->prepareToPlay(rate, 64);
          juce::AudioBuffer<float> b(2, 64);
          juce::MidiBuffer midi;
          std::vector<double> times;
          times.reserve(1000);
          for (int k = 0; k < 1200; ++k) {
            for (int c = 0; c < 2; ++c)
              for (int i = 0; i < 64; ++i)
                b.setSample(c, i, source("synth", k * 64 + i, rate));
            auto start = std::chrono::steady_clock::now();
            p->processBlock(b, midi);
            auto us = std::chrono::duration<double, std::micro>(
                          std::chrono::steady_clock::now() - start)
                          .count();
            if (k >= 200)
              times.push_back(us);
          }
          std::sort(times.begin(), times.end());
          cpu << rate << "," << mode << "," << hq << ","
              << (version == 0 ? "Phase5" : "Phase6") << "," << times[500]
              << "," << times[950] << "\n";
          p->releaseResources();
        }
  std::cout << "Phase5 archived DLL exact regression maximum difference "
            << worst << "\n";
}
} // namespace
int runPhase6Tests(const char *path) {
  bankAndFiles();
  playback();
  renders();
  archived(path);
  check(allocations == 0, "Phase6 allocations");
  std::cout << "Phase6: " << count
            << " assertions passed; callback allocations " << allocations
            << "\n";
  return count;
}
