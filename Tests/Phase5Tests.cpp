#include "PluginProcessor.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
extern thread_local bool track;
extern int allocations;
namespace {
int count = 0;
constexpr double pi = 3.14159265358979323846;
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
  auto f = root().getChildFile("Docs/Phase5");
  check(f.createDirectory().wasOk(), "Phase5 directory");
  return f;
}
void run(disruption::HQProcessor &e, juce::AudioBuffer<float> &b,
         const disruption::Controls &c, bool bypass = false) {
  track = true;
  e.process(b, c, bypass);
  track = false;
}
float signal(int type, int frame, double rate) {
  double t = frame / rate;
  float x = static_cast<float>(std::sin(2 * pi * 137 * t));
  switch (type) {
  case 0:
    return 0;
  case 1:
    return 1e-8f * x;
  case 2:
    return x;
  case 3:
    return x >= 0 ? 1.f : -1.f;
  case 4:
    return static_cast<float>(2 * std::fmod(137 * t, 1) - 1);
  case 5:
    return frame == 0 ? 1.f : 0.f;
  case 6:
    return 1.f;
  case 7:
    return 1000 * x;
  case 8:
    return static_cast<float>(.3 * std::sin(2 * pi * 55 * t) +
                              .1 * std::sin(2 * pi * 110 * t));
  case 9:
    return static_cast<float>(.2 * std::sin(2 * pi * 180 * t) +
                              .1 * std::sin(2 * pi * 540 * t) +
                              .05 * std::sin(2 * pi * 1260 * t));
  default:
    return static_cast<float>(std::sin(2 * pi * (20 * t + 10000 * t * t)));
  }
}
void filters() {
  std::ofstream csv(docs()
                        .getChildFile("filter-configuration.csv")
                        .getFullPathName()
                        .toStdString());
  csv << "stage,up_width,up_stop_db,up_order,down_width,down_stop_db,down_"
         "order,host_latency\n";
  int stage = 0;
  for (auto parameters : {std::array<float, 4>{.05f, -90, .06f, -75},
                          std::array<float, 4>{.10f, -80, .12f, -70}}) {
    auto up = juce::dsp::FilterDesign<
        float>::designFIRLowpassHalfBandEquirippleMethod(parameters[0],
                                                         parameters[1]);
    auto down = juce::dsp::FilterDesign<
        float>::designFIRLowpassHalfBandEquirippleMethod(parameters[2],
                                                         parameters[3]);
    double latency = (up->getFilterOrder() + down->getFilterOrder()) * .5 /
                     std::pow(2, stage + 1);
    csv << stage + 1 << "," << parameters[0] << "," << parameters[1] << ","
        << up->getFilterOrder() << "," << parameters[2] << "," << parameters[3]
        << "," << down->getFilterOrder() << "," << latency << "\n";
    ++stage;
  }
  check(csv.good(), "filter config csv");
}
void matrix() {
  std::ofstream csv(docs()
                        .getChildFile("rate-block-results.csv")
                        .getFullPathName()
                        .toStdString());
  csv << "rate,block,channels,cases,peak,latency,result\n";
  for (double rate : {44100., 48000., 88200., 96000.})
    for (int block : {32, 64, 128, 256, 512, 1024})
      for (int channels : {1, 2}) {
        double peak = 0;
        int cases = 0;
        for (int mode = 0; mode < 5; ++mode)
          for (float tone : {-100.f, 100.f})
            for (int type = 0; type < 11; ++type) {
              disruption::Controls c;
              c.mode = mode;
              c.drive = 36;
              c.damage = 100;
              c.tone = tone;
              c.body = 100;
              c.crush = 100;
              c.hq = true;
              disruption::HQProcessor e;
              e.prepare(rate, c);
              check(e.latency() == 60, "fixed HQ latency");
              juce::AudioBuffer<float> b(channels, block);
              for (int k = 0; k < 4; ++k) {
                for (int ch = 0; ch < channels; ++ch)
                  for (int i = 0; i < block; ++i)
                    b.setSample(ch, i,
                                signal(type, k * block + i, rate) *
                                    (ch ? -1 : 1));
                run(e, b, c);
                for (int ch = 0; ch < channels; ++ch)
                  for (int i = 0; i < block; ++i) {
                    float x = b.getSample(ch, i);
                    check(std::isfinite(x) && std::abs(x) < 12,
                          "HQ extreme finite bounded");
                    if (type == 0)
                      check(x == 0, "HQ silence");
                    peak = std::max(peak, static_cast<double>(std::abs(x)));
                  }
              }
              ++cases;
            }
        csv << rate << "," << block << "," << channels << "," << cases << ","
            << peak << ",60,PASS\n";
      }
  check(csv.good(), "HQ matrix csv");
}
void timing() {
  std::ofstream csv(
      docs().getChildFile("mix-timing.csv").getFullPathName().toStdString());
  csv << "rate,hq,mix,hz,gain_db,phase_error_deg\n";
  for (double rate : {44100., 48000., 88200., 96000.})
    for (bool hq : {false, true})
      for (float mix : {0.f, 25.f, 50.f, 75.f, 100.f}) {
        disruption::Controls c;
        c.hq = hq;
        c.mix = mix;
        disruption::HQProcessor e;
        e.prepare(rate, c);
        juce::AudioBuffer<float> b(1, 128);
        for (int k = 0; k < 2; ++k) {
          b.clear();
          if (k == 0)
            b.setSample(0, 0, .0001f);
          run(e, b, c);
          if (mix == 0)
            for (int i = 0; i < 128; ++i)
              check(b.getSample(0, i) == (k == 0 && i == 60 ? .0001f : 0.f),
                    "dry impulse delay");
        }
        for (double hz : {40., 250., 1000., 5000., 10000., 15000., 18000.}) {
          e.prepare(rate, c);
          double re = 0, im = 0, in = 0;
          int frames = static_cast<int>(rate);
          for (int k = 0; k < (frames + 127) / 128; ++k) {
            for (int i = 0; i < 128; ++i)
              b.setSample(
                  0, i,
                  static_cast<float>(
                      .001 * std::sin(2 * pi * hz * (k * 128 + i) / rate)));
            run(e, b, c);
            for (int i = 0; i < 128; ++i) {
              int n = k * 128 + i;
              if (n < frames / 2 || n >= frames)
                continue;
              double phase = 2 * pi * hz * (n - 60) / rate;
              re += b.getSample(0, i) * std::sin(phase);
              im += b.getSample(0, i) * std::cos(phase);
              in += .001 * std::sin(phase) * std::sin(phase);
            }
          }
          double gain = 20 * std::log10(std::hypot(re, im) / in),
                 error = std::atan2(im, re) * 180 / pi;
          check(std::abs(gain) < .5, "HQ mix comb amplitude");
          check(std::abs(error) < 8, "HQ linear phase timing");
          csv << rate << "," << hq << "," << mix << "," << hz << "," << gain
              << "," << error << "\n";
        }
        // Host and parameter bypass both preserve the reported delay exactly.
        c.bypass = true;
        c.drive = 36;
        c.damage = 100;
        c.tone = -100;
        c.body = 100;
        c.crush = 100;
        e.prepare(rate, c);
        for (int k = 0; k < 10; ++k) {
          for (int i = 0; i < 128; ++i)
            b.setSample(0, i, signal(9, k * 128 + i, rate));
          run(e, b, c);
          for (int i = 0; i < 128; ++i) {
            int n = k * 128 + i - 60;
            check(b.getSample(0, i) == (n < 0 ? 0.f : signal(9, n, rate)),
                  "HQ compensated bypass");
          }
        }
      }
  check(csv.good(), "timing CSV");
}
void automation() {
  std::ofstream csv(
      docs().getChildFile("automation.csv").getFullPathName().toStdString());
  csv << "rate,mode,peak,max_step,block_step,allocations\n";
  for (double rate : {44100., 48000., 88200., 96000.})
    for (int mode = 0; mode < 5; ++mode) {
      disruption::Controls c;
      c.mode = mode;
      c.drive = 12;
      c.damage = 65;
      c.mix = 50;
      disruption::HQProcessor e;
      e.prepare(rate, c);
      juce::AudioBuffer<float> b(2, 64);
      float last = 0;
      double peak = 0, step = 0, boundary = 0;
      int initial = allocations;
      for (int k = 0; k < 240; ++k) {
        c.hq = (k / 7) % 2 != 0;
        if (k % 11 == 0) {
          c.mode = (mode + k / 11) % 5;
          c.drive = k % 22 ? 0 : 36;
          c.damage = k % 22 ? 0 : 100;
          c.tone = k % 22 ? -100 : 100;
          c.body = k % 22 ? 0 : 100;
          c.crush = k % 22 ? 0 : 100;
          c.mix = k % 22 ? 25 : 75;
          c.output = k % 22 ? -24 : 12;
          c.bypass = (k / 11) % 3 == 0;
        }
        for (int ch = 0; ch < 2; ++ch)
          for (int i = 0; i < 64; ++i)
            b.setSample(ch, i, .1f);
        run(e, b, c);
        for (int i = 0; i < 64; ++i) {
          float x = b.getSample(0, i);
          check(std::isfinite(x) && std::abs(x) < 12, "HQ automation finite");
          double jump = std::abs(x - last);
          peak = std::max(peak, static_cast<double>(std::abs(x)));
          step = std::max(step, jump);
          if (i == 0)
            boundary = std::max(boundary, jump);
          last = x;
        }
      }
      check(allocations == initial, "HQ toggle allocation");
      csv << rate << "," << mode << "," << peak << "," << step << ","
          << boundary << "," << allocations - initial << "\n";
    }
  // Isolated toggles with a steady low-frequency signal, with reference
  // A continuous 130 Hz input exercises cold-start crossfades as well as steady
  // transitions.
  for (int mode = 0; mode < 5; ++mode) {
    disruption::Controls c;
    c.mode = mode;
    c.drive = 6;
    c.damage = 50;
    disruption::HQProcessor e;
    e.prepare(48000, c);
    juce::AudioBuffer<float> b(1, 64);
    float last = 0;
    double maxBoundary = 0;
    for (int k = 0; k < 600; ++k) {
      c.hq = (k / 40) % 2 != 0;
      for (int i = 0; i < 64; ++i)
        b.setSample(0, i,
                    static_cast<float>(
                        .1 * std::sin(2 * pi * 130 * (k * 64 + i) / 48000)));
      run(e, b, c);
      if (k > 20)
        maxBoundary =
            std::max(maxBoundary,
                     static_cast<double>(std::abs(b.getSample(0, 0) - last)));
      last = b.getSample(0, 63);
    }
    check(maxBoundary < .03, "isolated HQ toggle click");
  }
  check(csv.good(), "automation csv");
}
void aliasing() {
  constexpr int order = 14, N = 1 << order;
  juce::dsp::FFT fft(order);
  std::ofstream csv(
      docs().getChildFile("aliasing.csv").getFullPathName().toStdString());
  csv << "rate,mode,requested_hz,actual_hz,hq,alias_rms,total_rms,peak,alias_"
         "to_total_db\n";
  std::ofstream spectra(
      docs().getChildFile("spectra.csv").getFullPathName().toStdString());
  spectra << "rate,mode,requested_hz,hq,hz,amplitude_db\n";
  std::array<double, 5> improvement{};
  std::array<int, 5> cases{};
  for (double rate : {44100., 48000., 88200., 96000.})
    for (int mode = 0; mode < 5; ++mode)
      for (double requested : {250., 1000., 5000., 10000., 15000.}) {
        int bin = static_cast<int>(std::round(requested * N / rate));
        double hz = bin * rate / N;
        std::array<double, 2> alias{};
        for (int hq = 0; hq < 2; ++hq) {
          disruption::Controls c;
          c.mode = mode;
          c.drive = 36;
          c.damage = 100;
          c.hq = hq != 0;
          disruption::HQProcessor e;
          e.prepare(rate, c);
          juce::AudioBuffer<float> b(1, 128);
          std::vector<float> data(2 * N, 0);
          double energy = 0, peak = 0;
          const int warmBlocks = static_cast<int>(std::ceil(rate * 1.5 / 128));
          for (int k = 0; k < warmBlocks + N / 128; ++k) {
            for (int i = 0; i < 128; ++i)
              b.setSample(
                  0, i,
                  static_cast<float>(
                      .3 * std::sin(2 * pi * hz * (k * 128 + i) / rate)));
            run(e, b, c);
            if (k >= warmBlocks)
              for (int i = 0; i < 128; ++i) {
                float x = b.getSample(0, i);
                data[(k - warmBlocks) * 128 + i] = x;
                energy += x * x;
                peak = std::max(peak, static_cast<double>(std::abs(x)));
              }
          }
          fft.performFrequencyOnlyForwardTransform(data.data());
          double unwanted = 0, total = 0;
          for (int j = 1; j < N / 2; ++j) {
            double p = data[j] * data[j];
            total += p;
            bool legitimate = j % bin == 0;
            if (!legitimate)
              unwanted += p;
            if (rate == 48000 && requested >= 5000)
              spectra << rate << "," << mode << "," << requested << "," << hq
                      << "," << j * rate / N << ","
                      << 20 * std::log10(std::max(1e-12, 2.0 * data[j] / N))
                      << "\n";
          }
          alias[hq] = std::sqrt(2 * unwanted) / N;
          csv << rate << "," << mode << "," << requested << "," << hz << ","
              << hq << "," << alias[hq] << "," << std::sqrt(energy / N) << ","
              << peak << ","
              << 10 * std::log10(
                          std::max(1e-24, total > 0 ? unwanted / total : 0.0))
              << "\n";
          check(std::isfinite(alias[hq]), "spectral finite");
        }
        if (mode != 3) {
          improvement[mode] += 20 * std::log10(std::max(1e-12, alias[0]) /
                                               std::max(1e-12, alias[1]));
          ++cases[mode];
        } else
          check(std::abs(alias[0] - alias[1]) < 1e-5,
                "CRUSH intentional alias changed");
      }
  for (int mode : {0, 1, 2, 4}) {
    double db = improvement[mode] / cases[mode];
    std::cout << "HQ alias average reduction mode " << mode << ": " << db
              << " dB\n";
    check(db > 0, "HQ failed to reduce aliasing overall");
  }
  check(csv.good() && spectra.good(), "spectral csv");
}
void cpu() {
  std::ofstream csv(
      docs().getChildFile("cpu-benchmark.csv").getFullPathName().toStdString());
  csv << "rate,mode,hq,median_us,p95_us,core_percent\n";
  for (double rate : {48000., 96000.})
    for (int mode = 0; mode < 5; ++mode)
      for (bool hq : {false, true}) {
        disruption::Controls c;
        c.mode = mode;
        c.drive = 18;
        c.damage = 75;
        c.tone = 50;
        c.body = 60;
        c.crush = 60;
        c.hq = hq;
        disruption::HQProcessor e;
        e.prepare(rate, c);
        juce::AudioBuffer<float> b(2, 64);
        std::vector<double> times;
        times.reserve(1000);
        for (int k = 0; k < 1200; ++k) {
          for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 64; ++i)
              b.setSample(ch, i, signal(9, k * 64 + i, rate));
          track = true;
          auto start = std::chrono::steady_clock::now();
          e.process(b, c);
          auto end = std::chrono::steady_clock::now();
          track = false;
          if (k >= 200)
            times.push_back(
                std::chrono::duration<double, std::micro>(end - start).count());
        }
        std::sort(times.begin(), times.end());
        csv << rate << "," << mode << "," << hq << "," << times[500] << ","
            << times[950] << "," << times[500] * rate / 64 / 10000 << "\n";
      }
  check(csv.good(), "HQ CPU CSV");
}
void hostRegression(const char *path) {
  juce::AudioPluginFormatManager manager;
  manager.addFormat(std::make_unique<juce::VST3PluginFormat>());
  auto load = [&](const juce::File &file) {
    juce::OwnedArray<juce::PluginDescription> descriptions;
    manager.getFormat(0)->findAllTypesForFile(descriptions,
                                              file.getFullPathName());
    check(descriptions.size() == 1, "Phase5 VST3 discovery");
    juce::String err;
    auto p = manager.createPluginInstance(*descriptions[0], 48000, 128, err);
    check(p != nullptr, "Phase5 hosted load");
    return p;
  };
  auto old = load(root().getChildFile("build/Phase4Baseline/DISRUPTION.vst3"));
  auto current = load(juce::File(path));
  double worst = 0;
  std::ofstream csv(docs()
                        .getChildFile("phase4-regression.csv")
                        .getFullPathName()
                        .toStdString());
  csv << "rate,channels,mode,setting,maximum_difference,latency,result\n";
  for (double rate : {44100., 48000., 88200., 96000.})
    for (int channels : {1, 2})
      for (int mode = 0; mode < 5; ++mode)
        for (int setting = 0; setting < 6; ++setting) {
          for (auto *p : {old.get(), current.get()}) {
            for (auto *param : p->getParameters())
              param->setValueNotifyingHost(0);
            p->getParameters()[0]->setValueNotifyingHost(mode / 4.f);
            p->getParameters()[1]->setValueNotifyingHost(setting == 0   ? 0
                                                         : setting >= 2 ? 1
                                                                        : .5f);
            p->getParameters()[2]->setValueNotifyingHost(setting == 0   ? 0
                                                         : setting >= 2 ? 1
                                                                        : .75f);
            p->getParameters()[3]->setValueNotifyingHost(setting == 0  ? .5f
                                                         : setting % 2 ? .25f
                                                                       : 1);
            p->getParameters()[4]->setValueNotifyingHost(setting == 0 ? 0
                                                                      : .75f);
            p->getParameters()[5]->setValueNotifyingHost(setting == 0   ? 0
                                                         : setting >= 2 ? 1
                                                                        : .6f);
            p->getParameters()[6]->setValueNotifyingHost(setting == 4   ? 0
                                                         : setting == 1 ? .5f
                                                         : setting == 3 ? .25f
                                                                        : 1);
            p->getParameters()[7]->setValueNotifyingHost(2.f / 3);
            juce::AudioProcessor::BusesLayout l;
            auto layout = channels == 1 ? juce::AudioChannelSet::mono()
                                        : juce::AudioChannelSet::stereo();
            l.inputBuses.add(layout);
            l.outputBuses.add(layout);
            check(p->setBusesLayout(l), "Phase5 hosted layout");
          }
          juce::MemoryBlock state;
          old->getStateInformation(state);
          current->setStateInformation(state.getData(),
                                       static_cast<int>(state.getSize()));
          for (int i = 0; i < 10; ++i)
            check(old->getParameters()[i]->getValue() ==
                      current->getParameters()[i]->getValue(),
                  "Phase4 hosted state compatibility");
          old->prepareToPlay(rate, 128);
          current->prepareToPlay(rate, 128);
          check(old->getLatencySamples() == 0 &&
                    current->getLatencySamples() == 60,
                "hosted HQ-off latency");
          juce::AudioBuffer<float> a(channels, 128), b(channels, 128);
          juce::MidiBuffer midi;
          std::array<std::array<float, 60>, 2> delay{};
          int pos = 0;
          double maximum = 0;
          for (int k = 0; k < 64; ++k) {
            for (int ch = 0; ch < channels; ++ch)
              for (int i = 0; i < 128; ++i)
                a.setSample(ch, i,
                            signal(9, k * 128 + i, rate) * (ch ? -1 : 1));
            b.makeCopyOf(a, true);
            old->processBlock(a, midi);
            current->processBlock(b, midi);
            for (int i = 0; i < 128; ++i) {
              for (int ch = 0; ch < channels; ++ch) {
                float ref = delay[ch][pos];
                delay[ch][pos] = a.getSample(ch, i);
                double delta = std::abs(ref - b.getSample(ch, i));
                maximum = std::max(maximum, delta);
                check(delta < 1e-6, "Phase4 actual HQ-OFF regression");
              }
              pos = (pos + 1) % 60;
            }
          }
          worst = std::max(worst, maximum);
          csv << rate << "," << channels << "," << mode << "," << setting << ","
              << maximum << ",60,PASS\n";
          // Real VST3 receives repeated HQ changes and reports the same
          // compensation.
          for (int k = 0; k < 12; ++k) {
            current->getParameters()[8]->setValueNotifyingHost(k % 2 ? 1.f
                                                                     : 0.f);
            for (int ch = 0; ch < channels; ++ch)
              for (int i = 0; i < 128; ++i)
                b.setSample(ch, i, signal(9, k * 128 + i, rate));
            current->processBlock(b, midi);
            check(current->getLatencySamples() == 60,
                  "hosted HQ toggle latency");
            for (int ch = 0; ch < channels; ++ch)
              for (int i = 0; i < 128; ++i)
                check(std::isfinite(b.getSample(ch, i)),
                      "hosted HQ toggle finite");
          }
          old->releaseResources();
          current->releaseResources();
        }

  std::ofstream cpu(docs()
                        .getChildFile("archive-cpu-comparison.csv")
                        .getFullPathName()
                        .toStdString());
  cpu << "rate,mode,version,median_us,p95_us,core_percent\n";
  for (double rate : {48000., 96000.})
    for (int mode = 0; mode < 5; ++mode)
      for (int version = 0; version < 3; ++version) {
        auto *p = version == 0 ? old.get() : current.get();
        for (auto *parameter : p->getParameters())
          parameter->setValueNotifyingHost(0);
        p->getParameters()[0]->setValueNotifyingHost(mode / 4.f);
        p->getParameters()[1]->setValueNotifyingHost(.5f);
        p->getParameters()[2]->setValueNotifyingHost(.75f);
        p->getParameters()[3]->setValueNotifyingHost(.75f);
        p->getParameters()[4]->setValueNotifyingHost(.6f);
        p->getParameters()[5]->setValueNotifyingHost(.6f);
        p->getParameters()[6]->setValueNotifyingHost(1);
        p->getParameters()[7]->setValueNotifyingHost(2.f / 3);
        p->getParameters()[8]->setValueNotifyingHost(version == 2 ? 1.f : 0.f);
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        check(p->setBusesLayout(layout), "archive CPU layout");
        p->prepareToPlay(rate, 64);
        juce::AudioBuffer<float> b(2, 64);
        juce::MidiBuffer midi;
        std::vector<double> times;
        times.reserve(1000);
        for (int k = 0; k < 1200; ++k) {
          for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 64; ++i)
              b.setSample(ch, i, signal(9, k * 64 + i, rate));
          auto begin = std::chrono::steady_clock::now();
          p->processBlock(b, midi);
          double us = std::chrono::duration<double, std::micro>(
                          std::chrono::steady_clock::now() - begin)
                          .count();
          if (k >= 200)
            times.push_back(us);
        }
        std::sort(times.begin(), times.end());
        cpu << rate << "," << mode << ","
            << (version == 0   ? "Phase4"
                : version == 1 ? "HQ_OFF"
                               : "HQ_ON")
            << "," << times[500] << "," << times[950] << ","
            << times[500] * rate / 64 / 10000 << "\n";
        p->releaseResources();
      }
  check(cpu.good(), "archive CPU CSV");
  std::cout << "Phase4 archived DLL HQ-OFF, aligned 60 samples: max difference "
            << worst << "\n";
  check(csv.good(), "Phase4 regression CSV");
}
struct LatencyListener : juce::AudioProcessorListener {
  int notifications = 0;
  void audioProcessorParameterChanged(juce::AudioProcessor *, int,
                                      float) override {}
  void audioProcessorChanged(juce::AudioProcessor *,
                             const ChangeDetails &d) override {
    if (d.latencyChanged)
      ++notifications;
  }
};
void lifecycle() {
  LatencyListener listener;
  DisruptionProcessor processor;
  processor.addListener(&listener);
  processor.prepareToPlay(48000, 32);
  check(processor.getLatencySamples() == 60 && listener.notifications == 1,
        "host latency notification on prepare");
  juce::AudioBuffer<float> b(2, 32);
  juce::MidiBuffer midi;
  for (int k = 0; k < 20; ++k) {
    processor.parameters.getParameter("hq")->setValueNotifyingHost(k % 2 ? 1.f
                                                                         : 0.f);
    b.clear();
    track = true;
    processor.processBlock(b, midi);
    track = false;
  }
  check(listener.notifications == 1, "HQ callback notified host unnecessarily");
  processor.removeListener(&listener);
  // Identical stream partitioning, mono vs stereo, and reset/reprepare.
  for (bool hq : {false, true})
    for (int mode = 0; mode < 5; ++mode) {
      disruption::Controls c;
      c.mode = mode;
      c.hq = hq;
      c.drive = 12;
      c.damage = 65;
      c.tone = -35;
      c.body = 60;
      c.crush = 55;
      disruption::HQProcessor a, big, monoLeft, monoRight;
      a.prepare(48000, c);
      big.prepare(48000, c);
      monoLeft.prepare(48000, c);
      monoRight.prepare(48000, c);
      juce::AudioBuffer<float> full(2, 4096), reference(2, 4096);
      for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 4096; ++i)
          full.setSample(ch, i, signal(9, i, 48000) * (ch ? -1 : 1));
      reference.makeCopyOf(full, true);
      run(big, full, c);
      for (int offset = 0; offset < 4096; offset += 32) {
        float *ptr[2] = {reference.getWritePointer(0, offset),
                         reference.getWritePointer(1, offset)};
        juce::AudioBuffer<float> chunk(ptr, 2, 32);
        run(a, chunk, c);
      }
      for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 4096; ++i)
          check(full.getSample(ch, i) == reference.getSample(ch, i),
                "HQ block partition dependence");
      juce::AudioBuffer<float> l(1, 4096), r(1, 4096);
      for (int i = 0; i < 4096; ++i) {
        l.setSample(0, i, signal(9, i, 48000));
        r.setSample(0, i, -signal(9, i, 48000));
      }
      run(monoLeft, l, c);
      run(monoRight, r, c);
      for (int i = 0; i < 4096; ++i) {
        check(l.getSample(0, i) == full.getSample(0, i),
              "HQ stereo left differs from mono");
        check(r.getSample(0, i) == full.getSample(1, i),
              "HQ stereo right differs from mono");
      }
      // Verify native-rate digital damage remains exact for dedicated CRUSH
      // even with all stages enabled.
      if (mode == 3) {
        auto d = c;
        d.hq = !c.hq;
        disruption::HQProcessor other;
        other.prepare(48000, d);
        juce::AudioBuffer<float> x(2, 4096);
        for (int ch = 0; ch < 2; ++ch)
          for (int i = 0; i < 4096; ++i)
            x.setSample(ch, i, signal(9, i, 48000) * (ch ? -1 : 1));
        run(other, x, d);
        for (int ch = 0; ch < 2; ++ch)
          for (int i = 0; i < 4096; ++i)
            check(x.getSample(ch, i) == full.getSample(ch, i),
                  "HQ altered CRUSH full stack");
      }
      for (double rate : {44100., 48000., 88200., 96000.}) {
        a.prepare(rate, c);
        a.reset();
        juce::AudioBuffer<float> x(1, 33);
        x.clear();
        run(a, x, c);
        check(x.getMagnitude(0, 33) == 0, "HQ reprepare/reset silence");
      }
    }
  // Oversampled 5 Hz RIP DC rejection: plateau should settle to zero at each
  // rate.
  for (double rate : {44100., 48000., 88200., 96000.}) {
    disruption::Controls c;
    c.mode = 2;
    c.drive = 36;
    c.damage = 100;
    c.hq = true;
    disruption::HQProcessor e;
    e.prepare(rate, c);
    juce::AudioBuffer<float> b(1, 128);
    for (int k = 0; k < static_cast<int>(rate * 2 / 128); ++k) {
      for (int i = 0; i < 128; ++i)
        b.setSample(0, i, .25f);
      run(e, b, c);
    }
    check(b.getMagnitude(0, 128) < 1e-4, "HQ RIP 5Hz DC rejection");
  }
}

void fullStackMix() {
  for (double rate : {44100., 48000., 88200., 96000.})
    for (int mode = 0; mode < 5; ++mode)
      for (bool hq : {false, true}) {
        disruption::Controls c;
        c.mode = mode;
        c.hq = hq;
        c.drive = 36;
        c.damage = 100;
        c.tone = -100;
        c.body = 100;
        c.crush = 100;
        c.mix = 0;
        disruption::HQProcessor e;
        e.prepare(rate, c);
        juce::AudioBuffer<float> b(2, 128);
        for (int k = 0; k < 16; ++k) {
          for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 128; ++i)
              b.setSample(ch, i, signal(9, k * 128 + i, rate) * (ch ? -1 : 1));
          run(e, b, c);
          for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 128; ++i) {
              int n = k * 128 + i - 60;
              float raw = n < 0 ? 0.f : signal(9, n, rate) * (ch ? -1 : 1);
              check(b.getSample(ch, i) == raw, "HQ full-stage MIX0 dry leak");
            }
        }
      }
  std::ofstream csv(docs()
                        .getChildFile("full-stack-mix.csv")
                        .getFullPathName()
                        .toStdString());
  csv << "rate,mode,mix,max_error\n";
  for (double rate : {44100., 48000., 88200., 96000.})
    for (int mode = 0; mode < 5; ++mode)
      for (float mix : {25.f, 50.f, 75.f}) {
        disruption::Controls c;
        c.mode = mode;
        c.hq = true;
        c.drive = 12;
        c.damage = 65;
        c.tone = 35;
        c.body = 60;
        c.crush = 60;
        auto d = c;
        d.mix = mix;
        disruption::HQProcessor wet, partial;
        wet.prepare(rate, c);
        partial.prepare(rate, d);
        juce::AudioBuffer<float> a(1, 128), b(1, 128);
        float pole = static_cast<float>(std::exp(-2 * pi * 5 / rate)),
              previousInput = 0, previousOutput = 0;
        double error = 0;
        for (int k = 0; k < 32; ++k) {
          for (int i = 0; i < 128; ++i)
            a.setSample(0, i, signal(9, k * 128 + i, rate));
          b.makeCopyOf(a, true);
          run(wet, a, c);
          run(partial, b, d);
          for (int i = 0; i < 128; ++i) {
            int n = k * 128 + i - 60;
            float raw = n < 0 ? 0.f : signal(9, n, rate);
            float dc = raw - previousInput + pole * previousOutput;
            previousInput = raw;
            previousOutput = std::abs(dc) < 1e-20f ? 0.f : dc;
            float reference =
                a.getSample(0, i) * (mix * .01f) + dc * (1 - mix * .01f);
            error = std::max(error, static_cast<double>(std::abs(
                                        reference - b.getSample(0, i))));
            check(std::abs(reference - b.getSample(0, i)) < 1e-4,
                  "HQ full wet/dry chain alignment");
          }
        }
        csv << rate << "," << mode << "," << mix << "," << error << "\n";
      }
  check(csv.good(), "full-stack mix CSV");
}
void ripCutoff() {
  std::ofstream csv(docs()
                        .getChildFile("rip-dc-response.csv")
                        .getFullPathName()
                        .toStdString());
  csv << "rate,hq,hz,gain_db\n";
  for (double rate : {44100., 48000., 88200., 96000.})
    for (bool hq : {false, true}) {
      disruption::Controls c;
      c.mode = 2;
      c.hq = hq;
      disruption::HQProcessor e;
      e.prepare(rate, c);
      juce::AudioBuffer<float> b(1, 128);
      double re = 0, im = 0, in = 0;
      for (int k = 0; k < static_cast<int>(rate * 4 / 128); ++k) {
        for (int i = 0; i < 128; ++i)
          b.setSample(0, i,
                      static_cast<float>(
                          .0001 * std::sin(2 * pi * 5 * (k * 128 + i) / rate)));
        run(e, b, c);
        if (k * 128 >= rate * 2)
          for (int i = 0; i < 128; ++i) {
            double phase = 2 * pi * 5 * (k * 128 + i - 60) / rate;
            re += b.getSample(0, i) * std::sin(phase);
            im += b.getSample(0, i) * std::cos(phase);
            in += .0001 * std::sin(phase) * std::sin(phase);
          }
      }
      double gain = 20 * std::log10(std::hypot(re, im) / in);
      check(std::abs(gain - (20 * std::log10(1.12) - 6.0206)) < .08,
            "RIP oversampled DC cutoff moved");
      csv << rate << "," << hq << ",5," << gain << "\n";
    }
  check(csv.good(), "RIP response CSV");
}
void listening() {
  juce::WavAudioFormat format;
  auto reader = std::unique_ptr<juce::AudioFormatReader>(format.createReaderFor(
      root()
          .getChildFile("Docs/Phase2/Listening-Source.wav")
          .createInputStream()
          .release(),
      true));
  check(reader != nullptr, "Phase5 source reader");
  int frames = static_cast<int>(reader->lengthInSamples);
  juce::AudioBuffer<float> source(2, frames);
  check(reader->read(&source, 0, frames, 0, true, true), "Phase5 source read");
  auto write = [&](const juce::String &name, juce::AudioBuffer<float> &audio) {
    auto file = docs().getChildFile(name);
    auto stream = std::make_unique<juce::FileOutputStream>(file);
    check(stream->openedOk(), "Phase5 WAV open");
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
    check(writer && writer->writeFromAudioSampleBuffer(audio, 0,
                                                       audio.getNumSamples()),
          "Phase5 WAV write");
    writer.reset();
    auto verify = std::unique_ptr<juce::AudioFormatReader>(
        format.createReaderFor(file.createInputStream().release(), true));
    check(verify && verify->usesFloatingPointData &&
              verify->lengthInSamples == audio.getNumSamples(),
          "Phase5 float WAV verify");
    juce::Range<float> ranges[2];
    verify->readMaxLevels(0, audio.getNumSamples(), ranges, 2);
    float peak = 0;
    for (auto range : ranges)
      peak = std::max(
          peak, std::max(std::abs(range.getStart()), std::abs(range.getEnd())));
    check(std::abs(peak - audio.getMagnitude(0, audio.getNumSamples())) < 1e-6,
          "HQ float WAV peak");
  };
  std::ofstream csv(docs()
                        .getChildFile("listening-levels.csv")
                        .getFullPathName()
                        .toStdString());
  csv << "mode,stack,hq,drive,damage,tone,body,crush,raw_rms,raw_peak,offline_"
         "gain_db\n";
  const char *names[] = {"BURN", "BREAK", "RIP", "CRUSH", "CHAOS"};
  for (int mode = 0; mode < 5; ++mode)
    for (bool stack : {false, true}) {
      juce::AudioBuffer<float> pair(2, frames * 2);
      for (bool hq : {false, true}) {
        disruption::Controls c;
        c.mode = mode;
        c.drive = stack ? 18 : 24;
        c.damage = stack ? 75 : 90;
        c.hq = hq;
        c.tone = stack ? (mode == 2 ? 35 : -35) : 0;
        c.body = stack ? 60 : 0;
        c.crush = stack ? (mode == 0 ? 20 : mode == 2 ? 55 : 35) : 0;
        disruption::HQProcessor e;
        e.prepare(reader->sampleRate, c);
        juce::AudioBuffer<float> audio(source);
        run(e, audio, c);
        audio.applyGainRamp(0, 480, 0, 1);
        audio.applyGainRamp(frames - 480, 480, 1, 0);
        double energy = 0;
        for (int ch = 0; ch < 2; ++ch)
          for (int i = 0; i < frames; ++i)
            energy += std::pow(audio.getSample(ch, i), 2);
        double rms = std::sqrt(energy / (2 * frames));
        check(rms > 0, "HQ audible render");
        float gain = static_cast<float>(.1 / rms);
        csv << mode << "," << stack << "," << hq << "," << c.drive << ","
            << c.damage << "," << c.tone << "," << c.body << "," << c.crush
            << "," << rms << "," << audio.getMagnitude(0, frames) << ","
            << 20 * std::log10(gain) << "\n";
        juce::String stem = juce::String(names[mode]) +
                            (stack ? "-Stack" : "-Engine") +
                            (hq ? "-HQ-ON" : "-HQ-OFF");
        write(stem + "-Raw.wav", audio);
        audio.applyGain(gain);
        check(audio.getMagnitude(0, frames) < 1, "HQ matched peak");
        write(stem + "-RMS-matched.wav", audio);
        for (int ch = 0; ch < 2; ++ch)
          pair.copyFrom(ch, hq ? frames : 0, audio, ch, 0, frames);
      }
      write(juce::String(names[mode]) + (stack ? "-Stack" : "-Engine") +
                "-OFF-then-ON.wav",
            pair);
    }
  check(csv.good(), "listening level CSV");
}

} // namespace
int runPhase5Tests(const char *path) {
  filters();
  matrix();
  timing();
  automation();
  aliasing();
  cpu();
  hostRegression(path);
  lifecycle();
  fullStackMix();
  ripCutoff();
  listening();
  check(allocations == 0, "Phase5 callback allocations");
  std::cout << "Phase5: " << count
            << " assertions passed; callback allocations " << allocations
            << "\n";
  return count;
}
