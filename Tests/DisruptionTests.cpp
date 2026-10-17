#include "PluginProcessor.h"
#include "Phase2Tests.h"
#include "Phase3Tests.h"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <cmath>
#include <cstdlib>
#include <new>
int runPhase4Tests(const char*);
int runPhase5Tests(const char*);
thread_local bool track=false;
int allocations=0;
void* operator new(std::size_t n) { if(track) ++allocations; if(auto* p=std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
namespace {
int checks=0;
void require(bool v,const char* msg) { ++checks; if(!v) throw std::runtime_error(msg); }
void set(DisruptionProcessor& p,const char* id,float v) { auto* x=p.parameters.getParameter(id); x->setValueNotifyingHost(x->convertTo0to1(v)); }
void layout(juce::AudioProcessor& p,int n) {
 juce::AudioProcessor::BusesLayout l; auto s=n==1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo(); l.inputBuses.add(s); l.outputBuses.add(s); require(p.setBusesLayout(l),"layout rejected");
}
void finite(const juce::AudioBuffer<float>& b) { for(int c=0;c<b.getNumChannels();++c) for(int i=0;i<b.getNumSamples();++i) require(std::isfinite(b.getSample(c,i)),"non-finite output"); }
void process(DisruptionProcessor& p,juce::AudioBuffer<float>& b,bool bypass=false) { juce::MidiBuffer midi; track=true; if(bypass) p.processBlockBypassed(b,midi); else p.processBlock(b,midi); track=false; finite(b); }
void matrix() {
 for(double rate : {44100.0,48000.0,88200.0,96000.0}) for(int channels : {1,2}) {
  DisruptionProcessor p; layout(p,channels); p.prepareToPlay(rate,128); juce::AudioBuffer<float> b(channels,128);
  b.clear(); process(p,b); require(b.getMagnitude(0,128)==0,"silence altered");
  for(int block=0;block<160;++block) {
   for(auto* parameter:p.getParameters()) parameter->setValueNotifyingHost(block%2 ? 1.0f : 0.0f);
   set(p,"bypass",0);
   for(int c=0;c<channels;++c) for(int i=0;i<128;++i) b.setSample(c,i,0.2f*std::sin(static_cast<float>((block*128+i)*0.04+c)));
   process(p,b);
  }
  set(p,"hq",0); set(p,"tone",0); set(p,"body",0); set(p,"crush",0); set(p,"mode",0); set(p,"damage",0); set(p,"drive",6); set(p,"output",0); set(p,"mix",0); set(p,"bypass",0); p.prepareToPlay(rate,128);
  for(int c=0;c<channels;++c) for(int i=0;i<128;++i) b.setSample(c,i,c==0 ? 0.25f : -0.125f);
  process(p,b); require(std::abs(b.getSample(0,127)-0.25f)<1e-6f,"dry mix not transparent");
  set(p,"mix",100); p.prepareToPlay(rate,128);
  // Impulse verifies DRIVE gain and channel independence without DC ambiguity.
  b.clear(); b.setSample(0,0,0.25f); process(p,b);
  require(std::abs(b.getSample(0,p.getLatencySamples())-disruption::BurnEngine::transfer(0.25f*juce::Decibels::decibelsToGain(6.0f),0))<1e-5f,"wet gain wrong");
  if(channels==2) require(b.getMagnitude(1,0,128)==0,"channel crosstalk");
  set(p,"bypass",1); p.prepareToPlay(rate,128); for(int c=0;c<channels;++c) for(int i=0;i<128;++i) b.setSample(c,i,0.2f);
  process(p,b); require(std::abs(b.getSample(0,127)-0.2f)<1e-6f,"parameter bypass wrong");
  set(p,"bypass",0); p.prepareToPlay(rate,128);
  for(int block=0;block<20;++block) { for(int c=0;c<channels;++c) for(int i=0;i<128;++i) b.setSample(c,i,0.2f); process(p,b,true); }
  require(std::abs(b.getSample(0,127)-0.2f)<1e-6f,"host bypass wrong");
  b.clear(); b.setSample(0,0,std::numeric_limits<float>::quiet_NaN()); b.setSample(0,1,std::numeric_limits<float>::infinity()); b.setSample(0,2,std::numeric_limits<float>::max()); process(p,b);
  juce::AudioBuffer<float> large(channels,2049); large.clear(); process(p,large);
 }
 require(allocations==0,"heap allocation in callback");
}
void smoothingAndMix() {
 DisruptionProcessor p; layout(p,1); set(p,"hq",0); set(p,"tone",0); set(p,"body",0); set(p,"crush",0); set(p,"mode",0); set(p,"damage",0); set(p,"drive",6); set(p,"mix",50); set(p,"output",6); p.prepareToPlay(48000,128);
 juce::AudioBuffer<float> b(1,128); b.clear(); b.setSample(0,0,0.1f); process(p,b);
 const float gain=juce::Decibels::decibelsToGain(6.0f);
 require(std::abs(b.getSample(0,p.getLatencySamples())-(0.1f+disruption::BurnEngine::transfer(0.1f*gain,0))*0.5f*gain)<1e-5f,"partial mix or post-mix output wrong");
 set(p,"drive",0); set(p,"mix",100); set(p,"output",0); p.prepareToPlay(48000,128);
 set(p,"drive",36); for(int i=0;i<128;++i) b.setSample(0,i,i%2 ? -0.25f : 0.25f); process(p,b);
 require(std::abs(b.getSample(0,0))<0.3f,"drive jumps without smoothing");
 for(int block=0;block<12;++block) { for(int i=0;i<128;++i) b.setSample(0,i,i%2 ? -0.25f : 0.25f); process(p,b); }
 require(std::abs(std::abs(b.getSample(0,127))-disruption::BurnEngine::transfer(0.25f*juce::Decibels::decibelsToGain(36.0f),0))<0.005f,"drive smoothing never reaches target");
 set(p,"output",-24); for(int i=0;i<128;++i) b.setSample(0,i,i%2 ? -0.25f : 0.25f); process(p,b);
 require(std::abs(b.getSample(0,0))>1.3f,"output jumps without smoothing");
 p.parameters.getRawParameterValue("drive")->store(std::numeric_limits<float>::quiet_NaN());
 p.parameters.getRawParameterValue("mode")->store(std::numeric_limits<float>::infinity());
 b.clear(); process(p,b);
 juce::AudioBuffer<float> unsupported(3,128); unsupported.clear(); unsupported.setSample(0,0,1); process(p,unsupported);
 require(unsupported.getMagnitude(0,128)==0,"unsupported buffer not cleared");
}
void stateTests() {
 DisruptionProcessor p; int i=0; for(auto* param:p.getParameters()) { ++i; param->setValueNotifyingHost(i>=9 ? 1.0f : (i==1 ? 0.5f : static_cast<float>(i)/10.0f)); }
 juce::MemoryBlock state; p.getStateInformation(state); DisruptionProcessor restored; restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
 for(int j=0;j<p.getParameters().size();++j) { if(std::abs(p.getParameters()[j]->getValue()-restored.getParameters()[j]->getValue())>=1e-5f) std::cerr << "State mismatch " << j << ": " << p.getParameters()[j]->getValue() << " -> " << restored.getParameters()[j]->getValue() << " XML: " << juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize()))->toString() << "\n"; require(std::abs(p.getParameters()[j]->getValue()-restored.getParameters()[j]->getValue())<1e-5f,"state mismatch"); }
 restored.setStateInformation("bad",3); require(restored.parameters.getRawParameterValue("mix")->load()==p.parameters.getRawParameterValue("mix")->load(),"invalid state changed parameters");
 auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize()));
 require(xml!=nullptr,"state XML missing"); auto tree=juce::ValueTree::fromXml(*xml);
 tree.getChildWithProperty("id","drive").setProperty("value",999999.0,nullptr);
 tree.getChildWithProperty("id","output").setProperty("value",std::numeric_limits<double>::infinity(),nullptr);
 auto malformed=tree.createXml(); juce::MemoryBlock bad; juce::AudioProcessor::copyXmlToBinary(*malformed,bad); restored.setStateInformation(bad.getData(),static_cast<int>(bad.getSize()));
 require(restored.parameters.getRawParameterValue("drive")->load()<=36,"invalid range accepted");
 require(std::isfinite(restored.parameters.getRawParameterValue("output")->load()),"invalid state finite protection");
 juce::AudioProcessor::BusesLayout l; l.inputBuses.add(juce::AudioChannelSet::stereo()); l.outputBuses.add(juce::AudioChannelSet::mono()); require(!p.isBusesLayoutSupported(l),"mismatch supported");
 l.inputBuses.set(0,juce::AudioChannelSet::create5point1()); l.outputBuses.set(0,juce::AudioChannelSet::create5point1()); require(!p.isBusesLayoutSupported(l),"surround supported");
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor()); require(editor && editor->getWidth()==900 && editor->getHeight()==600,"editor initialization");
 auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds()); juce::File image=juce::File::getCurrentWorkingDirectory().getChildFile("editor-preview.png"); juce::FileOutputStream stream(image); require(stream.openedOk(),"preview file failed"); stream.setPosition(0); stream.truncate(); juce::PNGImageFormat png; require(png.writeImageToStream(snapshot,stream),"preview encode failed");
}
void hostTest(const char* path) {
 juce::AudioPluginFormatManager manager; manager.addFormat(std::make_unique<juce::VST3PluginFormat>()); juce::OwnedArray<juce::PluginDescription> descriptions;
 for(auto* format:manager.getFormats()) format->findAllTypesForFile(descriptions,path);
 require(descriptions.size()==1,"VST3 discovery failed"); require(!descriptions[0]->isInstrument,"plugin is instrument");
 juce::String error; auto plugin=manager.createPluginInstance(*descriptions[0],48000,128,error);
 require(plugin!=nullptr,"VST3 loading failed"); require(plugin->getParameters().size()>=10,"host parameters missing");
 for(int channels:{1,2}) { layout(*plugin,channels); plugin->prepareToPlay(48000,128); juce::AudioBuffer<float> b(channels,128); b.clear(); b.setSample(0,0,0.25f); juce::MidiBuffer midi; plugin->processBlock(b,midi); finite(b); require(b.getMagnitude(0,128)>0,"host audio missing"); plugin->releaseResources(); }
 juce::MemoryBlock state; plugin->getParameters()[1]->setValueNotifyingHost(0.5f); plugin->getStateInformation(state); plugin->getParameters()[1]->setValueNotifyingHost(0.0f); plugin->setStateInformation(state.getData(),static_cast<int>(state.getSize())); require(std::abs(plugin->getParameters()[1]->getValue()-0.5f)<1e-5f,"host state restore failed");
 std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditor()); require(editor!=nullptr,"host editor failed");

 std::cout<<"Actual VST3 discovery, load, mono/stereo, state and editor: PASS\n";
}
}
int runPhase8Tests(const char*);
int runPhase8Clean(const char*);
int runPhase7Profile();
int runPhase7Tests(const char*);
int runPhase6Tests(const char*);
int main(int argc,char** argv) { juce::ScopedJuceInitialiser_GUI gui; try { if(argc>2 && juce::String(argv[2])=="--clean-location"){runPhase8Clean(argv[1]);return 0;} if(argc>2 && juce::String(argv[2])=="--phase8-only"){runPhase8Tests(argv[1]);return 0;} if(argc>2 && juce::String(argv[2])=="--phase6-only"){auto result=runPhase6Tests(argv[1]);std::cout<<result<<" Phase6 checks passed\n";return 0;} if(argc>2 && juce::String(argv[2])=="--phase7-profile"){runPhase7Profile();return 0;} if(argc>2 && juce::String(argv[2])=="--phase7-only"){auto result=runPhase7Tests(argv[1]);std::cout<<result<<" Phase7 checks passed\n";return 0;} DisruptionProcessor probe; probe.prepareToPlay(48000,128); std::cout<<"Phase5 fixed latency "<<probe.getLatencySamples()<<" samples\n"; matrix(); smoothingAndMix(); stateTests(); require(argc>1,"missing VST3 path"); hostTest(argv[1]); checks+=runPhase2Tests(argv[1]); checks+=runPhase3Tests(argv[1]); checks+=runPhase4Tests(argv[1]); checks+=runPhase5Tests(argv[1]); checks+=runPhase6Tests(argv[1]); checks+=runPhase7Tests(argv[1]); checks+=runPhase8Tests(argv[1]); std::cout<<checks<<" checks passed; realtime allocations: "<<allocations<<"\n"; return 0; } catch(const std::exception& e) { track=false; std::cerr<<e.what()<<"\n"; return 1; } }













