#include "PresetBar.h"
namespace disruption::ui {
PresetBar::PresetBar(DisruptionProcessor &p) : processor(p) {
  for (auto *button : {&previous, &next, &save, &remove})
    addAndMakeVisible(*button);
  addAndMakeVisible(presetBox);
  addAndMakeVisible(presetStatus);
  presetStatus.setJustificationType(juce::Justification::centred);
  previous.onClick = [this] {
    showResult(processor.presetManager().previous());
  };
  next.onClick = [this] { showResult(processor.presetManager().next()); };
  presetBox.onChange = [this] {
    int i = presetBox.getSelectedId() - 1;
    if (i >= 0 && i < static_cast<int>(presetEntries.size()))
      showResult(processor.presetManager().load(presetEntries[i].id));
  };
  save.onClick = [this] { savePreset(); };
  remove.onClick = [this] {
    auto id = processor.presetManager().currentId();
    juce::Component::SafePointer<PresetBar> safe(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Delete user preset?")
            .withMessage("Delete " + processor.presetManager().currentName() +
                         "? Factory presets are protected.")
            .withButton("Delete")
            .withButton("Cancel"),
        [safe, id](int result) {
          if (safe && result == 1)
            safe->showResult(safe->processor.presetManager().deleteUser(id));
        });
  };
  auto errors = processor.presetManager().refreshUsers();
  refreshPresets();
  if (!errors.isEmpty())
    presetStatus.setTooltip(errors.joinIntoString("\n"));
  presetBox.setComponentID("presets");
  presetBox.setTooltip(
      "Factory and user presets. * indicates modified settings.");
  save.setTooltip("Save the current settings as a user preset.");
  remove.setTooltip("Delete the current user preset after confirmation.");
  presetStatus.setColour(juce::Label::textColourId, juce::Colour(0xffa6aaad));
  startTimerHz(10);
}
void PresetBar::resized() {
  previous.setBounds(0, 0, 28, 36);
  presetBox.setBounds(34, 0, getWidth() - 68, 36);
  next.setBounds(getWidth() - 28, 0, 28, 36);
  presetStatus.setBounds(0, 40, getWidth() - 116, 22);
  save.setBounds(getWidth() - 110, 42, 54, 22);
  remove.setBounds(getWidth() - 50, 42, 50, 22);
}
void PresetBar::sync() {
  auto &manager = processor.presetManager();
  auto display = manager.currentName() + (manager.isModified() ? " *" : "");
  presetStatus.setText(manager.currentId().startsWith("factory.")
                           ? "FACTORY BANK"
                           : "USER / PROJECT",
                       juce::dontSendNotification);
  bool deletable = false;
  for (auto &p : presetEntries)
    if (p.id == manager.currentId() && !p.factory)
      deletable = true;
  remove.setEnabled(deletable);
  if (!manager.isModified())
    for (size_t i = 0; i < presetEntries.size(); ++i)
      if (presetEntries[i].id == manager.currentId())
        presetBox.setSelectedId(static_cast<int>(i) + 1,
                                juce::dontSendNotification);
  if (presetBox.getText() != display)
    presetBox.setText(display, juce::dontSendNotification);
}
void PresetBar::refreshPresets() {
  presetEntries = processor.presetManager().entries();
  presetBox.clear(juce::dontSendNotification);
  juce::String category;
  int i = 0;
  for (auto &p : presetEntries) {
    if (p.category != category) {
      category = p.category;
      presetBox.addSectionHeading(category);
    }
    presetBox.addItem(p.name, ++i);
    if (p.id == processor.presetManager().currentId())
      presetBox.setSelectedId(i, juce::dontSendNotification);
  }
  sync();
}
void PresetBar::showResult(juce::Result result) {
  if (result.failed())
    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::WarningIcon, "Preset",
        result.getErrorMessage());
  refreshPresets();
}
void PresetBar::savePreset() {
  auto *dialog = new juce::AlertWindow("Save user preset",
                                       "Enter a preset name (1-64 characters).",
                                       juce::MessageBoxIconType::NoIcon);
  dialog->addTextEditor("name", processor.presetManager().currentName());
  dialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
  dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
  juce::Component::SafePointer<PresetBar> safe(this);
  dialog->enterModalState(
      true, juce::ModalCallbackFunction::create([safe, dialog](int result) {
        if (!safe || result != 1)
          return;
        auto name = dialog->getTextEditorContents("name").trim();
        safe->processor.presetManager().refreshUsers();
        bool exists = false;
        for (auto &p : safe->processor.presetManager().entries())
          if (!p.factory && p.name.equalsIgnoreCase(name))
            exists = true;
        if (!exists) {
          safe->showResult(safe->processor.presetManager().saveUser(name));
          return;
        }
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::QuestionIcon)
                .withTitle("Overwrite user preset?")
                .withMessage("Replace " + name + " with the current settings?")
                .withButton("Overwrite")
                .withButton("Cancel"),
            [safe, name](int answer) {
              if (safe && answer == 1)
                safe->showResult(
                    safe->processor.presetManager().saveUser(name, true));
            });
      }),
      true);
}

} // namespace disruption::ui
