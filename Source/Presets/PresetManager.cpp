#include "PresetManager.h"
#include <BinaryData.h>
#include <cstdlib>
#include <set>
namespace disruption {
namespace {
constexpr const char *ids[] = {"mode",  "drive", "damage", "tone", "body",
                               "crush", "mix",   "output", "hq",   "bypass"};
constexpr float low[] = {0, 0, 0, -100, 0, 0, 0, -24, 0, 0},
                high[] = {4, 36, 100, 100, 100, 100, 100, 12, 1, 1};
bool legalName(const juce::String &name) {
  if (name.isEmpty() || name.length() > 64 || name == "." || name == "..")
    return false;
  for (auto c : name)
    if (c < 32 || juce::String("<>:\"/\\|?*").containsChar(c))
      return false;
  return true;
}
} // namespace
juce::Result PresetManager::parse(const juce::XmlElement &xml, Preset &output) {
  if (!xml.hasTagName("DISRUPTION_PRESET") ||
      xml.getStringAttribute("plugin") != "DISRUPTION" ||
      xml.getStringAttribute("manufacturer") != "VoxxLab")
    return juce::Result::fail("Not a DISRUPTION preset.");
  if (xml.getStringAttribute("version") != "1")
    return juce::Result::fail("Unsupported preset format version.");
  Preset p;
  p.name = xml.getStringAttribute("name").trim();
  if (!legalName(p.name))
    return juce::Result::fail(
        "Invalid preset name (use 1-64 readable characters).");
  p.id = xml.getStringAttribute("id");
  p.category = xml.getStringAttribute("category", "User").substring(0, 64);
  p.author = xml.getStringAttribute("author", "User").substring(0, 64);
  p.source = xml.getStringAttribute("source", "custom").substring(0, 64);
  std::set<juce::String> seen;
  for (auto *child : xml.getChildIterator()) {
    if (!child->hasTagName("PARAM"))
      continue;
    auto id = child->getStringAttribute("id");
    int index = -1;
    for (int i = 0; i < 10; ++i)
      if (id == ids[i])
        index = i;
    if (index < 0)
      continue;
    if (!seen.insert(id).second)
      return juce::Result::fail("Duplicate parameter: " + id);
    auto text = child->getStringAttribute("value").trim();
    const char *start = text.toRawUTF8();
    char *end = nullptr;
    float value = std::strtof(start, &end);
    if (end == start || *end != 0 || !std::isfinite(value))
      return juce::Result::fail("Invalid parameter: " + id);
    if (index == 0 || index >= 8) {
      if (value != std::floor(value) || value < low[index] ||
          value > high[index])
        return juce::Result::fail("Invalid choice or boolean: " + id);
    }
    p.values[static_cast<size_t>(index)] =
        juce::jlimit(low[index], high[index], value);
  }
  output = std::move(p);
  return juce::Result::ok();
}
std::unique_ptr<juce::XmlElement> PresetManager::serialize(const Preset &p) {
  auto xml = std::make_unique<juce::XmlElement>("DISRUPTION_PRESET");
  xml->setAttribute("version", 1);
  xml->setAttribute("id", p.id);
  xml->setAttribute("name", p.name);
  xml->setAttribute("category", p.category);
  xml->setAttribute("author", p.author);
  xml->setAttribute("source", p.source);
  xml->setAttribute("manufacturer", "VoxxLab");
  xml->setAttribute("plugin", "DISRUPTION");
  for (int i = 0; i < 10; ++i) {
    auto *child = xml->createNewChildElement("PARAM");
    child->setAttribute("id", ids[i]);
    child->setAttribute("value", static_cast<double>(p.values[i]));
  }
  return xml;
}
PresetManager::PresetManager(juce::AudioProcessorValueTreeState &s)
    : state(s), directory(defaultUserDirectory()) {
  auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(
      BinaryData::FactoryPresets_xml, BinaryData::FactoryPresets_xmlSize));
  if (xml)
    for (auto *child : xml->getChildIterator()) {
      Preset p;
      if (parse(*child, p).wasOk()) {
        p.factory = true;
        factories.push_back(std::move(p));
      }
    }
  current.name = "INIT";
  current.id = "factory.init";
  current.category = "INIT";
  current.factory = true;
  current.author = "VoxxLab";
  current.values = snapshot();
}
juce::File PresetManager::defaultUserDirectory() {
  return juce::File::getSpecialLocation(
             juce::File::userApplicationDataDirectory)
      .getChildFile("VoxxLab/DISRUPTION/Presets");
}
std::array<float, 10> PresetManager::snapshot() const {
  std::array<float, 10> result{};
  for (int i = 0; i < 10; ++i)
    result[i] =
        state.getRawParameterValue(ids[i])->load(std::memory_order_relaxed);
  return result;
}
std::vector<Preset> PresetManager::entries() const {
  const juce::ScopedLock lock(metadataLock);
  auto all = factories;
  all.insert(all.end(), users.begin(), users.end());
  return all;
}
void PresetManager::setUserDirectory(const juce::File &path) {
  const juce::ScopedLock lock(metadataLock);
  directory = path;
  users.clear();
}
juce::File PresetManager::userDirectory() const {
  const juce::ScopedLock lock(metadataLock);
  return directory;
}
juce::StringArray PresetManager::refreshUsers() {
  const juce::ScopedLock lock(metadataLock);
  std::vector<Preset> found;
  juce::StringArray errors;
  std::set<juce::String> seen;
  auto files = directory.findChildFiles(juce::File::findFiles, false,
                                        "*.disruptionpreset");
  files.sort();
  for (auto file : files) {
    if (file.getSize() > 1024 * 1024) {
      errors.add(file.getFileName() + ": file too large");
      continue;
    }
    auto xml = juce::XmlDocument::parse(file);
    Preset p;
    auto result = xml ? parse(*xml, p) : juce::Result::fail("Invalid XML");
    if (result.failed()) {
      errors.add(file.getFileName() + ": " + result.getErrorMessage());
      continue;
    }
    p.id = "user." + file.getFileNameWithoutExtension();
    p.file = file;
    p.factory = false;
    if (!seen.insert(p.name.toLowerCase()).second) {
      errors.add("Duplicate user name: " + p.name);
      continue;
    }
    found.push_back(std::move(p));
  }
  std::sort(found.begin(), found.end(), [](const Preset &a, const Preset &b) {
    int compare = a.name.compareIgnoreCase(b.name);
    return compare == 0 ? a.id < b.id : compare < 0;
  });
  users = std::move(found);
  return errors;
}
juce::Result PresetManager::apply(const Preset &p) {
  // Files/parsing are complete before this batch; audio uses its last stable
  // snapshot while odd.
  const juce::ScopedLock lock(metadataLock);
  revision.fetch_add(1, std::memory_order_acq_rel);
  for (int i = 0; i < 10; ++i) {
    auto *parameter = state.getParameter(ids[i]);
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(p.values[i]));
    parameter->endChangeGesture();
  }
  revision.fetch_add(1, std::memory_order_release);
  current = p;
  return juce::Result::ok();
}
juce::Result PresetManager::load(const juce::String &id) {
  for (auto &p : entries())
    if (p.id == id)
      return apply(p);
  return juce::Result::fail("Preset not found.");
}
juce::Result PresetManager::loadFile(const juce::File &file) {
  if (!file.existsAsFile() || file.getSize() > 1024 * 1024)
    return juce::Result::fail("Preset missing or too large.");
  auto xml = juce::XmlDocument::parse(file);
  Preset p;
  if (!xml)
    return juce::Result::fail("Invalid XML.");
  auto result = parse(*xml, p);
  if (result.failed())
    return result;
  p.file = file;
  p.id = "user." + file.getFileNameWithoutExtension();
  return apply(p);
}
juce::Result PresetManager::navigate(int direction) {
  auto all = entries();
  if (all.empty())
    return juce::Result::fail("No presets.");
  auto id = currentId();
  int index = -1;
  for (int i = 0; i < static_cast<int>(all.size()); ++i)
    if (all[i].id == id)
      index = i;
  int target = index < 0 ? 0
                         : (index + direction + static_cast<int>(all.size())) %
                               static_cast<int>(all.size());
  return apply(all[target]);
}
juce::Result PresetManager::previous() { return navigate(-1); }
juce::Result PresetManager::next() { return navigate(1); }
juce::Result PresetManager::saveUser(const juce::String &rawName,
                                     bool overwrite) {
  auto name = rawName.trim();
  if (!legalName(name))
    return juce::Result::fail(
        "Use 1-64 characters without path/control characters.");
  refreshUsers();
  Preset p;
  {
    const juce::ScopedLock lock(metadataLock);
    for (auto &user : users)
      if (user.name.equalsIgnoreCase(name)) {
        if (!overwrite)
          return juce::Result::fail(
              "A user preset with this name exists. Confirm overwrite.");
        p = user;
        break;
      }
    p.name = name;
    p.category = "User";
    p.author = "User";
    p.values = snapshot();
    p.factory = false;
    if (p.id.isEmpty()) {
      auto uuid = juce::Uuid().toString();
      p.id = "user." + uuid;
      p.file = directory.getChildFile(uuid + ".disruptionpreset");
    }
  }
  auto result = p.file.getParentDirectory().createDirectory();
  if (result.failed())
    return result;
  auto xml = serialize(p);
  juce::TemporaryFile temp(p.file);
  if (!xml->writeTo(temp.getFile()))
    return juce::Result::fail("Could not write temporary preset.");
  if (!temp.overwriteTargetFileWithTemporary())
    return juce::Result::fail("Could not replace preset file.");
  refreshUsers();
  {
    const juce::ScopedLock lock(metadataLock);
    current = p;
  }
  return juce::Result::ok();
}
juce::Result PresetManager::deleteUser(const juce::String &id) {
  Preset found;
  bool exists = false;
  {
    const juce::ScopedLock lock(metadataLock);
    for (auto &p : users)
      if (p.id == id) {
        found = p;
        exists = true;
        break;
      }
  }
  if (!exists)
    return juce::Result::fail("Factory presets cannot be deleted.");
  if (!found.file.deleteFile())
    return juce::Result::fail("Could not delete preset.");
  refreshUsers();
  return juce::Result::ok();
}
juce::String PresetManager::currentId() const {
  const juce::ScopedLock lock(metadataLock);
  return current.id;
}
bool PresetManager::isModified() const {
  auto values = snapshot();
  const juce::ScopedLock lock(metadataLock);
  for (int i = 0; i < 10; ++i)
    if (std::abs(values[i] - current.values[i]) > 0.0051f)
      return true;
  return false;
}
juce::String PresetManager::currentName() const {
  const juce::ScopedLock lock(metadataLock);
  return current.name;
}
juce::ValueTree PresetManager::copyProjectState() const {
  const juce::ScopedLock lock(metadataLock);
  auto tree = state.copyState();
  appendContext(tree);
  return tree;
}
void PresetManager::appendContext(juce::ValueTree &tree) const {
  const juce::ScopedLock lock(metadataLock);
  auto context = juce::ValueTree::fromXml(*serialize(current));
  context.setProperty("factory", current.factory, nullptr);
  juce::ValueTree container("PRESET_CONTEXT");
  container.addChild(context, -1, nullptr);
  tree.addChild(container, -1, nullptr);
}
void PresetManager::restoreState(const juce::ValueTree &tree,
                                 const juce::XmlElement &xml) {
  const juce::ScopedLock lock(metadataLock);
  revision.fetch_add(1, std::memory_order_acq_rel);
  state.replaceState(tree);
  restoreContext(xml);
  revision.fetch_add(1, std::memory_order_release);
}
void PresetManager::restoreContext(const juce::XmlElement &xml) {
  Preset p;
  p.name = "Custom";
  p.category = "Project";
  p.values = snapshot();
  if (auto *context = xml.getChildByName("PRESET_CONTEXT"))
    if (auto *child = context->getChildByName("DISRUPTION_PRESET")) {
      Preset parsed;
      if (parse(*child, parsed).wasOk()) {
        p = std::move(parsed);
        p.factory = child->getBoolAttribute("factory", false);
      }
    }
  const juce::ScopedLock lock(metadataLock);
  current = std::move(p);
}
} // namespace disruption
