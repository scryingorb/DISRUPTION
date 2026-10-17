#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstdlib>
namespace { constexpr const char* ids[] = {"mode","drive","damage","tone","body","crush","mix","output","hq","bypass"}; }
juce::AudioProcessorValueTreeState::ParameterLayout DisruptionProcessor::parameterLayout() {
 juce::AudioProcessorValueTreeState::ParameterLayout p;
 p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode",1},"Mode",juce::StringArray{"BURN","BREAK","RIP","CRUSH","CHAOS"},0));
 auto add=[&](const char* id,const char* name,float low,float high,float def,const char* unit) {
  p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{id,1},name,juce::NormalisableRange<float>{low,high,0.01f},def,juce::AudioParameterFloatAttributes{}.withLabel(unit)));
 };
 add("drive","Drive",0,36,0,"dB"); add("damage","Damage",0,100,0,"%");
 add("tone","Tone",-100,100,0,"%"); add("body","Body",0,100,0,"%"); add("crush","Crush",0,100,0,"%");
 add("mix","Mix",0,100,100,"%"); add("output","Output",-24,12,0,"dB");
 p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"hq",1},"HQ",false));
 p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false)); return p;
}
DisruptionProcessor::DisruptionProcessor() : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)), parameters(*this,nullptr,"DISRUPTION_STATE",parameterLayout()), presets(parameters) {
 for(size_t i=0;i<values.size();++i) values[i]=parameters.getRawParameterValue(ids[i]);
}
disruption::Controls DisruptionProcessor::controls() const noexcept {
 auto get=[&](size_t i,float lo,float hi,float def) { float v=values[i]->load(std::memory_order_relaxed); return std::isfinite(v) ? juce::jlimit(lo,hi,v) : def; };
 disruption::Controls c; c.mode=static_cast<int>(get(0,0,4,0)); c.drive=get(1,0,36,0); c.damage=get(2,0,100,0);
 c.tone=get(3,-100,100,0); c.body=get(4,0,100,0); c.crush=get(5,0,100,0); c.mix=get(6,0,100,100);
 c.output=get(7,-24,12,0); c.hq=get(8,0,1,0)>=0.5f; c.bypass=get(9,0,1,0)>=0.5f; return c;
}
bool DisruptionProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
 const auto out=l.getMainOutputChannelSet(); return (out==juce::AudioChannelSet::mono() || out==juce::AudioChannelSet::stereo()) && out==l.getMainInputChannelSet();
}
void DisruptionProcessor::prepareToPlay(double rate,int) { lastStableControls=controls(); engine.prepare(rate,lastStableControls); setLatencySamples(engine.latency()); }
void DisruptionProcessor::run(juce::AudioBuffer<float>& b,bool bypassed) {
 if(b.getNumChannels()!=getTotalNumOutputChannels() || getTotalNumInputChannels()!=getTotalNumOutputChannels()) { b.clear(); return; }
 const auto epoch=presets.revision.load(std::memory_order_acquire);
 if((epoch&1u)==0){const auto candidate=controls();std::atomic_thread_fence(std::memory_order_acquire);if(presets.revision.load(std::memory_order_acquire)==epoch)lastStableControls=candidate;}
 engine.process(b,lastStableControls,bypassed); visualBridge.push(b);
}
void DisruptionProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) { run(b,false); }
void DisruptionProcessor::processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer&) { run(b,true); }
void DisruptionProcessor::getStateInformation(juce::MemoryBlock& dest) {
 auto state=presets.copyProjectState(); state.setProperty("schemaVersion",1,nullptr); if(auto xml=state.createXml()) copyXmlToBinary(*xml,dest);
}
void DisruptionProcessor::setStateInformation(const void* data,int size) {
 if(data==nullptr || size<=0) return;
 auto xml=getXmlFromBinary(data,size); if(!xml || !xml->hasTagName("DISRUPTION_STATE")) return;
 auto incoming=juce::ValueTree::fromXml(*xml); if(!incoming.isValid()) return;
 // Merge only known, finite parameter values into a complete current schema.
 auto clean=parameters.copyState();
 for(const auto* id:ids) {
  auto child=incoming.getChildWithProperty("id",id); if(!child.isValid() || !child.hasProperty("value")) continue;
  const auto raw=child.getProperty("value");
  const auto text=raw.toString().trim(); const auto utf8=text.toRawUTF8(); char* end=nullptr;
  const float value=std::strtof(utf8,&end);
  if(end==utf8 || *end!=0 || !std::isfinite(value)) continue;
  auto* p=parameters.getParameter(id);
  const float legal=p->convertFrom0to1(juce::jlimit(0.0f,1.0f,p->convertTo0to1(value)));
  auto target=clean.getChildWithProperty("id",id); target.setProperty("value",legal,nullptr);
 }
 clean.setProperty("schemaVersion",1,nullptr); presets.restoreState(clean,*xml);
}
juce::AudioProcessorEditor* DisruptionProcessor::createEditor() { return new DisruptionEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DisruptionProcessor(); }

