#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "DSP/HQProcessor.h"
#include "Presets/PresetManager.h"
#include "UI/VisualizerBridge.h"
class DisruptionProcessor final : public juce::AudioProcessor {
public:
 DisruptionProcessor();
 void prepareToPlay(double,int) override;
 void releaseResources() override { engine.reset(); }
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override { return true; }
 const juce::String getName() const override { return "DISRUPTION"; }
 bool acceptsMidi() const override { return false; }
 bool producesMidi() const override { return false; }
 bool isMidiEffect() const override { return false; }
 double getTailLengthSeconds() const override { return 0; }
 int getNumPrograms() override { return 1; }
 int getCurrentProgram() override { return 0; }
 void setCurrentProgram(int) override {}
 const juce::String getProgramName(int) override { return {}; }
 void changeProgramName(int,const juce::String&) override {}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;
 juce::AudioProcessorParameter* getBypassParameter() const override { return parameters.getParameter("bypass"); }
 juce::AudioProcessorValueTreeState parameters;
 static juce::AudioProcessorValueTreeState::ParameterLayout parameterLayout();
 disruption::PresetManager& presetManager() noexcept { return presets; }
 disruption::VisualizerBridge& visualizerBridge() noexcept { return visualBridge; }
private:
 disruption::VisualizerBridge visualBridge;
 disruption::HQProcessor engine;
 disruption::PresetManager presets;
 disruption::Controls lastStableControls;
 std::array<std::atomic<float>*,10> values {};
 disruption::Controls controls() const noexcept;
 void run(juce::AudioBuffer<float>&,bool);
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DisruptionProcessor)
};
