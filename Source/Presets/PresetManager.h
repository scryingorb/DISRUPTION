#pragma once
#include <atomic>
#include <juce_audio_utils/juce_audio_utils.h>
namespace disruption {
struct Preset {
  juce::String id, name, category, author = "User", source = "custom";
  std::array<float, 10> values{{0, 0, 0, 0, 0, 0, 100, 0, 0, 0}};
  bool factory = false;
  juce::File file;
};
// All catalog, serialization and mutation methods are non-real-time.
// The callback only reads revision and keeps its own POD control snapshot.
class PresetManager {
public:
  explicit PresetManager(juce::AudioProcessorValueTreeState &);
  static juce::File defaultUserDirectory();
  static juce::Result parse(const juce::XmlElement &, Preset &);
  static std::unique_ptr<juce::XmlElement> serialize(const Preset &);
  const std::vector<Preset> &factoryPresets() const noexcept {
    return factories;
  }
  std::vector<Preset> entries() const;
  void
  setUserDirectory(const juce::File &); // Injected by tests; no callback use.
  juce::File userDirectory() const;
  juce::StringArray refreshUsers();
  juce::Result load(const juce::String &id);
  juce::Result loadFile(const juce::File &);
  juce::Result previous();
  juce::Result next();
  juce::Result saveUser(const juce::String &name, bool overwrite = false);
  juce::Result deleteUser(const juce::String &id);
  juce::String currentName() const;
  juce::String currentId() const;
  bool isModified() const;
  juce::ValueTree copyProjectState() const;
  void appendContext(juce::ValueTree &) const;
  void restoreState(const juce::ValueTree &, const juce::XmlElement &);
  void restoreContext(const juce::XmlElement &);
  std::atomic<uint32_t> revision{
      0}; // Audio reads only; odd means UI batch in progress.
private:
  juce::AudioProcessorValueTreeState &state;
  std::vector<Preset> factories, users;
  juce::File directory;
  Preset current;
  mutable juce::CriticalSection metadataLock; // Never acquired by processBlock.
  std::array<float, 10> snapshot() const;
  juce::Result apply(const Preset &);
  juce::Result navigate(int);
};
} // namespace disruption
