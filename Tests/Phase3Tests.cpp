#include "Phase3Tests.h"
#include "PluginProcessor.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
extern thread_local bool track;
extern int allocations;
namespace {
int count=0;
constexpr double pi=3.14159265358979323846;
const char* names[]={"BURN","BREAK","RIP","CRUSH","CHAOS"};
void check(bool valid,const char* message) { ++count; if(!valid) throw std::runtime_error(message); }
juce::File root() { return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getParentDirectory().getParentDirectory(); }
juce::File docs() { auto f=root().getChildFile("Docs/Phase3"); check(f.createDirectory().wasOk(),"Phase 3 directory"); return f; }
void set(juce::AudioProcessorValueTreeState& state,const char* id,float value) { auto* p=state.getParameter(id); p->setValueNotifyingHost(p->convertTo0to1(value)); }
void channels(juce::AudioProcessor& p,int n) { juce::AudioProcessor::BusesLayout l; const auto s=n==1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo(); l.inputBuses.add(s); l.outputBuses.add(s); check(p.setBusesLayout(l),"Phase 3 layout"); }
void run(disruption::DisruptionEngine& e,juce::AudioBuffer<float>& b,const disruption::Controls& c) { track=true; e.process(b,c); track=false; }
void run(DisruptionProcessor& p,juce::AudioBuffer<float>& b) { juce::MidiBuffer midi; track=true; p.processBlock(b,midi); track=false; }
float signal(int type,int frame,int channel) {
 const double phase=2*pi*frame/128.0+channel*0.37;
 const float s=static_cast<float>(std::sin(phase));
 switch(type) {
 case 0: return 0;
 case 1: return 1e-6f*s;
 case 2: return s;
 case 3: return s>=0 ? 1.0f : -1.0f;
 case 4: return static_cast<float>(2.0*std::fmod(frame/128.0+channel*0.12,1.0)-1.0);
 case 5: return frame==0 ? 1.0f : 0.0f;
 case 6: return 1.0f;
 case 7: return -1.0f;
 case 8: return std::abs(s);
 case 9: return -std::abs(s);
 case 10: return static_cast<float>(0.25*std::sin(phase)+0.12*std::sin(3*phase)+0.05*std::sin(7*phase));
 case 11: return 1000.0f*s;
 case 12: return s>=0 ? std::numeric_limits<float>::max() : -std::numeric_limits<float>::max();
 default: return frame%2 ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity();
 }
}
void extremeMatrix() {
 std::ofstream csv(docs().getChildFile("sample-rate-results.csv").getFullPathName().toStdString()); csv<<"sample_rate,cases,peak,assertions,result\n";
 const std::array<std::array<float,2>,9> settings{{{0,0},{36,0},{0,100},{36,100},{6,20},{18,50},{30,75},{36,20},{12,90}}};
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) {
  const int before=count; int cases=0; float peak=0;
  for(int mode:{3,4}) for(int n:{1,2}) for(const auto& setting:settings) for(int type=0;type<14;++type) {
   DisruptionProcessor p; channels(p,n); set(p.parameters,"mode",static_cast<float>(mode)); set(p.parameters,"drive",setting[0]); set(p.parameters,"damage",setting[1]); p.prepareToPlay(rate,128);
   juce::AudioBuffer<float> b(n,128); check(p.getLatencySamples()>0,"new engine latency");
   for(int block=0;block<8;++block) {
    for(int ch=0;ch<n;++ch) for(int i=0;i<128;++i) b.setSample(ch,i,signal(type,block*128+i,ch)); run(p,b);
    for(int ch=0;ch<n;++ch) for(int i=0;i<128;++i) { const float x=b.getSample(ch,i); check(std::isfinite(x),"Phase 3 extreme nonfinite"); check(std::abs(x)<4,"Phase 3 extreme unbounded"); peak=std::max(peak,std::abs(x)); if(type==0) check(x==0,"new engine silence changed"); }
   }
   ++cases;
  }
  csv<<rate<<","<<cases<<","<<peak<<","<<count-before<<",PASS\n"; std::cout<<"Phase 3 extreme matrix "<<rate<<" Hz: "<<cases<<" cases PASS, peak "<<peak<<"\n";
 }
 check(csv.good(),"extreme CSV");
 for(float d:{-1e30f,0.0f,100.0f,1e30f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) for(float x:{-std::numeric_limits<float>::max(),-1e-30f,0.0f,1e-30f,std::numeric_limits<float>::max(),std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
  track=true; const float q=disruption::CrushEngine::quantize(x,d),c=disruption::ChaosEngine::transfer(x,d); track=false;
  check(std::isfinite(q) && std::abs(q)<=1,"CRUSH transfer safety"); check(std::isfinite(c) && std::abs(c)<1.5,"CHAOS transfer safety");
 }
}
void holdClockAndQuantization() {
 std::ofstream csv(docs().getChildFile("crush-clock-mapping.csv").getFullPathName().toStdString()); csv<<"host_rate,damage,bits,requested_rate,effective_rate,captures_per_second,shortest_hold,longest_hold\n";
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) for(float d:{0.0f,20.0f,50.0f,75.0f,90.0f,100.0f}) {
  disruption::CrushEngine crush; crush.prepare(rate); int captures=0,last=-1,shortest=1000000,longest=0; float previous=0;
  for(int i=0;i<static_cast<int>(rate);++i) {
   track=true; crush.beginFrame(d); const float x=0.8f*static_cast<float>(std::sin(i*0.004)); const float l=crush.process(x,0,d),r=crush.process(-x,1,d); track=false;
   check(std::abs(l+r)<1e-6,"CRUSH stereo values not independent");
   if(crush.didCaptureThisFrame()) { ++captures; if(last>=0) { shortest=std::min(shortest,i-last); longest=std::max(longest,i-last); } last=i; }
   else check(l==previous,"CRUSH output changed inside constant-coefficient hold");
   previous=l;
  }
  const double requested=disruption::CrushEngine::requestedSampleRate(d),effective=std::min(rate,requested);
  check(std::abs(crush.getEffectiveRate()-effective)<1e-7,"CRUSH host-rate mapping"); check(std::abs(captures-effective)<=1.1,"CRUSH capture frequency wrong");
  check(shortest>=static_cast<int>(std::floor(rate/effective)) && longest<=static_cast<int>(std::ceil(rate/effective)),"CRUSH variable hold duration invalid");
  csv<<rate<<","<<d<<","<<disruption::CrushEngine::effectiveBits(d)<<","<<requested<<","<<effective<<","<<captures<<","<<shortest<<","<<longest<<"\n";
 }
 check(csv.good(),"hold clock CSV");
 // Adjacent-bit interpolation is continuous for a fixed input. Signal-dependent
 // quantization edges intentionally remain stepped and are checked separately.
 float previous=disruption::CrushEngine::quantize(0.37f,0); float maxStep=0;
 for(int i=1;i<=10000;++i) { const float y=disruption::CrushEngine::quantize(0.37f,i*0.01f); maxStep=std::max(maxStep,std::abs(y-previous)); previous=y; }
 check(maxStep<0.001,"CRUSH DAMAGE coefficient discontinuity");
 for(int i=0;i<=1000;++i) { const float y=disruption::CrushEngine::quantize(i*0.001f,100); check(std::abs(y*4-std::round(y*4))<1e-6,"CRUSH extreme quantization grid"); }
 check(disruption::CrushEngine::effectiveBits(0)==16 && disruption::CrushEngine::effectiveBits(100)==3,"bit depth endpoints");
 check(std::abs(disruption::CrushEngine::requestedSampleRate(100)-600)<1e-7,"hold frequency endpoint");
 disruption::CrushEngine p; p.prepare(48000); p.beginFrame(100); p.process(0.4f,0,100);
 for(int i=0;i<37;++i) { p.beginFrame(100); p.process(0.4f,0,100); }
 p.beginFrame(95); check(!p.didCaptureThisFrame(),"CRUSH phase reset on DAMAGE");
 p.reset(); p.beginFrame(100); check(p.didCaptureThisFrame(),"CRUSH reset first capture"); check(p.process(0,0,100)==0,"CRUSH reset stale input"); check(p.process(1,3,100)==0,"CRUSH invalid channel");
 // Stereo capture timing must equal two identically-clocked mono engines.
 for(double rate:{44100.0,96000.0}) { disruption::CrushEngine s,l,r; s.prepare(rate); l.prepare(rate); r.prepare(rate);
  for(int i=0;i<4096;++i) { const float d=i%1000*0.1f; s.beginFrame(d); l.beginFrame(d); r.beginFrame(d); const float x=signal(10,i,0),y=signal(10,i,1); check(s.process(x,0,d)==l.process(x,0,d) && s.process(y,1,d)==r.process(y,0,d),"mono/stereo CRUSH clock difference"); }
 }
 std::cout<<"CRUSH clock, host-rate consistency, stereo and quantization interpolation: PASS\n";
}
void foldingAndDC() {
 for(double t:{0.16,0.4,1.65}) {
  check(std::abs(disruption::ChaosEngine::fold(0,t))<1e-15,"fold origin");
  check(std::abs(disruption::ChaosEngine::fold(t,t)-t)<1e-14,"fold threshold");
  check(std::abs(disruption::ChaosEngine::fold(2*t,t))<1e-14,"fold return");
  check(std::abs(disruption::ChaosEngine::fold(3*t,t)+t)<1e-14,"fold negative peak");
  for(double x:{-std::numeric_limits<double>::max(),-1000000.0,-12.0,0.0,12.0,1000000.0,std::numeric_limits<double>::max()}) { const double y=disruption::ChaosEngine::fold(x,t); check(std::isfinite(y) && std::abs(y)<=t+1e-14,"extreme fold bound"); }
 }
 check(disruption::ChaosEngine::fold(1,0)==0 && disruption::ChaosEngine::fold(std::numeric_limits<double>::infinity(),1)==0,"invalid fold input");
 for(float d:{0.0f,25.0f,50.0f,75.0f,100.0f}) {
  check(disruption::ChaosEngine::transfer(0,d)==0,"CHAOS raw silence");
  float previous=disruption::ChaosEngine::transfer(-4,d); int reversals=0; float previousSlope=0;
  for(int i=1;i<=8000;++i) { const float y=disruption::ChaosEngine::transfer(-4+i*0.001f,d),slope=y-previous; check(std::abs(slope)<0.03,"CHAOS transfer discontinuity"); if(slope*previousSlope<0) ++reversals; if(std::abs(slope)>1e-6) previousSlope=slope; previous=y; }
  if(d>=50) check(reversals>=4,"CHAOS repeated folding absent");
 }
 check(std::abs(disruption::ChaosEngine::transfer(0.5f,100)+disruption::ChaosEngine::transfer(-0.5f,100))>0.05,"CHAOS asymmetry absent");
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) for(float d:{0.0f,50.0f,100.0f}) {
  disruption::ChaosEngine e; e.prepare(rate); e.beginFrame(d); double tail=0;
  const int frames=static_cast<int>(rate*1.5);
  for(int i=0;i<frames;++i) { track=true; const float y=e.process(1000,0,d),silent=e.process(0,1,d); track=false; check(silent==0,"CHAOS DC stereo leak"); if(i>frames-1024) tail=std::max(tail,static_cast<double>(std::abs(y))); }
  check(tail<1e-7,"CHAOS DC not removed"); e.reset(); e.beginFrame(d); check(e.process(0,0,d)==0,"CHAOS DC reset");
  for(int mode:{3,4}) { disruption::Controls c; c.mode=mode; c.damage=d; c.drive=36; disruption::DisruptionEngine full; full.prepare(rate,c); juce::AudioBuffer<float> b(1,128);
   float last=0; const int blocks=static_cast<int>(rate*1.5/128);
   for(int block=0;block<blocks;++block) { for(int i=0;i<128;++i) b.setSample(0,i,-1000); run(full,b,c); if(block>blocks-8) last=std::max(last,b.getMagnitude(0,128)); }
   check(last<0.0003f,"new full-chain DC not removed");
  }
 }
}
std::vector<float> render(int mode,float drive,float damage,int frames=8192,int partition=128,double rate=48000) {
 disruption::Controls c; c.mode=mode; c.drive=drive; c.damage=damage; disruption::DisruptionEngine e; e.prepare(rate,c);
 std::vector<float> result(static_cast<size_t>(frames)); juce::AudioBuffer<float> b(2,partition);
 for(int position=0;position<frames;) {
  const int length=std::min(partition,frames-position); juce::AudioBuffer<float> view(b.getArrayOfWritePointers(),2,length);
  for(int ch=0;ch<2;++ch) for(int i=0;i<length;++i) view.setSample(ch,i,signal(10,position+i,ch)); run(e,view,c);
  for(int i=0;i<length;++i) result[static_cast<size_t>(position+i)]=view.getSample(0,i); position+=length;
 }
 return result;
}
double difference(const std::vector<float>& a,const std::vector<float>& b) { double energy=0; for(size_t i=0;i<a.size();++i) energy+=std::pow(a[i]-b[i],2); return std::sqrt(energy/a.size()); }
void distinctnessAndDeterminism() {
 std::array<std::vector<float>,5> audio;
 std::ofstream csv(docs().getChildFile("five-mode-differences.csv").getFullPathName().toStdString()); csv<<"first,second,difference_rms,normalized_difference_rms\n";
 for(int mode=0;mode<5;++mode) {
  audio[static_cast<size_t>(mode)]=render(mode,12,75); const auto low=render(mode,12,0);
  check(difference(audio[static_cast<size_t>(mode)],low)>0.015,"DAMAGE fails to alter engine");
  const auto repeated=render(mode,12,75); check(audio[static_cast<size_t>(mode)]==repeated,"engine not deterministic");
 }
 for(int a=0;a<5;++a) for(int b=a+1;b<5;++b) { const double delta=difference(audio[static_cast<size_t>(a)],audio[static_cast<size_t>(b)]); check(delta>0.04,"five engines not distinct");  double aa=0,bb=0; for(size_t i=2048;i<audio[0].size();++i) { aa+=std::pow(audio[static_cast<size_t>(a)][i],2); bb+=std::pow(audio[static_cast<size_t>(b)][i],2); }
  aa=std::sqrt(aa/(audio[0].size()-2048)); bb=std::sqrt(bb/(audio[0].size()-2048)); double normalized=0;
  for(size_t i=2048;i<audio[0].size();++i) normalized+=std::pow(audio[static_cast<size_t>(a)][i]/aa-audio[static_cast<size_t>(b)][i]/bb,2);
  normalized=std::sqrt(normalized/(audio[0].size()-2048)); check(normalized>0.03,"five-mode difference is only level");
  csv<<names[a]<<","<<names[b]<<","<<delta<<","<<normalized<<"\n"; }
 for(int mode:{3,4}) {
  const auto a=render(mode,36,20),b=render(mode,12,90),c=render(mode,36,100);
  check(difference(a,b)>0.08 && difference(b,c)>0.08 && difference(a,c)>0.08,"new DRIVE/DAMAGE combinations redundant");
  for(double rate:{44100.0,48000.0,88200.0,96000.0}) { const auto reference=render(mode,12,75,4096,1,rate);
   for(int partition:{17,128,511}) check(reference==render(mode,12,75,4096,partition,rate),"block partition changes DSP");
  }
 }
 check(csv.good(),"difference CSV");
 // Global CRUSH, TONE, BODY and HQ must be audio-null in every mode.
 for(int mode=0;mode<5;++mode) {
  DisruptionProcessor a,b; for(auto* p:{&a,&b}) { set(p->parameters,"mode",static_cast<float>(mode)); set(p->parameters,"drive",12); set(p->parameters,"damage",75); }
  set(b.parameters,"hq",1); // HQ now has real processing; compare its intentional effect.
  a.prepareToPlay(48000,128); b.prepareToPlay(48000,128); juce::AudioBuffer<float> left(2,128),right(2,128); double hqDifference=0;
  for(int block=0;block<20;++block) { for(int ch=0;ch<2;++ch) for(int i=0;i<128;++i) left.setSample(ch,i,signal(10,block*128+i,ch)); right.copyFrom(0,0,left,0,0,128); right.copyFrom(1,0,left,1,0,128); run(a,left); run(b,right); for(int ch=0;ch<2;++ch) for(int i=0;i<128;++i) { hqDifference+=std::abs(left.getSample(ch,i)-right.getSample(ch,i)); check(std::isfinite(right.getSample(ch,i)),"HQ regression nonfinite"); } }
  check(mode==3 ? hqDifference==0 : hqDifference>1e-6,"HQ must filter nonlinear modes and preserve native CRUSH");
 }
}
void modeAndAutomation() {
 std::ofstream csv(docs().getChildFile("mode-switch-boundaries.csv").getFullPathName().toStdString()); csv<<"rate,from,to,first_residual,max_residual_step\n";
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) for(int from=0;from<5;++from) for(int to=0;to<5;++to) if(from!=to) {
  disruption::Controls c; c.mode=from; c.drive=6; c.damage=75; auto target=c; target.mode=to;
  disruption::DisruptionEngine reference,changed; reference.prepare(rate,c); changed.prepare(rate,c); juce::AudioBuffer<float> a(2,64),b(2,64);
  auto fill=[&] { for(int ch=0;ch<2;++ch) for(int i=0;i<64;++i) { a.setSample(ch,i,0.3f); b.setSample(ch,i,0.3f); } };
  for(int block=0;block<100;++block) { fill(); run(reference,a,c); run(changed,b,c); }
  double previous=0,first=0,maxStep=0;
  for(int block=0;block<20;++block) { fill(); run(reference,a,c); run(changed,b,target); for(int i=0;i<64;++i) { const double residual=b.getSample(0,i)-a.getSample(0,i); if(block==0 && i==0) first=std::abs(residual); maxStep=std::max(maxStep,std::abs(residual-previous)); previous=residual; } }
  check(first<0.03 && maxStep<0.03,"mode switch discontinuity"); csv<<rate<<","<<names[from]<<","<<names[to]<<","<<first<<","<<maxStep<<"\n";
  // Feed silence through the fade, then compare an impulse to a settled target.
  // No previous DC transient can disguise an unfinished 5 ms weight ramp.
  disruption::DisruptionEngine fading,settled; fading.prepare(rate,c); settled.prepare(rate,target); juce::AudioBuffer<float> x(1,1),y(1,1);
  for(int i=0;i<=static_cast<int>(rate*0.005);++i) { x.clear(); y.clear(); run(fading,x,target); run(settled,y,target); }
  x.setSample(0,0,0.37f); y.setSample(0,0,0.37f); run(fading,x,target); run(settled,y,target); check(std::abs(x.getSample(0,0)-y.getSample(0,0))<1e-6,"mode fade exceeds 5 ms");
 }
 check(csv.good(),"mode boundary CSV");
 std::ofstream boundaries(docs().getChildFile("automation-boundaries.csv").getFullPathName().toStdString()); boundaries<<"rate,mode,control,first_sample_residual\n";
 const char* controls[]={"drive","damage","mix","output","bypass"};
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) for(int mode:{3,4}) for(int action=0;action<5;++action) {
  disruption::Controls c; c.mode=mode; c.drive=0; c.damage=50; auto target=c;
  if(action==0) target.drive=36; else if(action==1) target.damage=100; else if(action==2) target.mix=0; else if(action==3) target.output=12; else target.bypass=true;
  disruption::DisruptionEngine reference,changed; reference.prepare(rate,c); changed.prepare(rate,c); juce::AudioBuffer<float> a(1,64),b(1,64);
  auto fill=[&] { for(int i=0;i<64;++i) { a.setSample(0,i,0.3f); b.setSample(0,i,0.3f); } };
  for(int block=0;block<100;++block) { fill(); run(reference,a,c); run(changed,b,c); }
  fill(); run(reference,a,c); run(changed,b,target); const float residual=std::abs(a.getSample(0,0)-b.getSample(0,0)); check(residual<0.03,"new automation boundary discontinuity"); boundaries<<rate<<","<<names[mode]<<","<<controls[action]<<","<<residual<<"\n";
 }
 check(boundaries.good(),"automation boundary CSV");
 // Desired quantization/hold updates later in a ramp are not click failures.
 // The coefficient continuity and phase-continuity tests cover those separately.
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) {
  DisruptionProcessor p; p.prepareToPlay(rate,8); juce::AudioBuffer<float> b(2,8);
  for(int block=0;block<5000;++block) {
   set(p.parameters,"mode",static_cast<float>(block%5)); set(p.parameters,"drive",block%2 ? 36.0f : 0.0f); set(p.parameters,"damage",block%3 ? 100.0f : 0.0f); set(p.parameters,"mix",block%7 ? 100.0f : 0.0f); set(p.parameters,"output",block%11 ? 12.0f : -24.0f); set(p.parameters,"bypass",block%13 ? 0.0f : 1.0f);
   for(int ch=0;ch<2;++ch) for(int i=0;i<8;++i) b.setSample(ch,i,signal(10,block*8+i,ch)); run(p,b);
   for(int ch=0;ch<2;++ch) for(int i=0;i<8;++i) check(std::isfinite(b.getSample(ch,i)) && std::abs(b.getSample(ch,i))<16,"Phase 3 torture unstable");
  }
  // Silence under fast automation must remain exact silence, including all folds.
  p.prepareToPlay(rate,8); for(int block=0;block<500;++block) { set(p.parameters,"mode",static_cast<float>(block%5)); set(p.parameters,"damage",static_cast<float>(block%101)); b.clear(); run(p,b); check(b.getMagnitude(0,8)==0,"automation invented signal on silence"); }
 }
 std::cout<<"All 20 directed mode transitions, 5 ms settling and automation torture: PASS\n";
}
void levelComparison() {
 std::ofstream csv(docs().getChildFile("five-mode-levels.csv").getFullPathName().toStdString()); csv<<"amplitude,drive,damage,burn_rms,break_rms,rip_rms,crush_rms,chaos_rms,spread_db\n"; double worst=0;
 for(float amplitude:{0.5f,1.0f}) for(float drive:{0.0f,12.0f,36.0f}) for(float damage:{0.0f,50.0f,100.0f}) {
  std::array<double,5> levels {};
  for(int mode=0;mode<5;++mode) {
   disruption::Controls c; c.mode=mode; c.drive=drive; c.damage=damage; disruption::DisruptionEngine e; e.prepare(48000,c); juce::AudioBuffer<float> b(1,128); double energy=0; int frames=0;
   for(int block=0;block<240;++block) { for(int i=0;i<128;++i) { const double phase=2*pi*(block*128+i)/128.0; b.setSample(0,i,amplitude*static_cast<float>(0.65*std::sin(phase)+0.2*std::sin(3*phase)+0.15*std::sin(7*phase))); } run(e,b,c); if(block>=120) { for(int i=0;i<128;++i) energy+=std::pow(b.getSample(0,i),2); frames+=128; } }
   levels[static_cast<size_t>(mode)]=std::sqrt(energy/frames); check(levels[static_cast<size_t>(mode)]>0.05,"representative level collapsed");
  }
  const double spread=20*std::log10(*std::max_element(levels.begin(),levels.end())/ *std::min_element(levels.begin(),levels.end())); worst=std::max(worst,spread);
  check(spread<8,"five-mode level spread excessive"); csv<<amplitude<<","<<drive<<","<<damage; for(double level:levels) csv<<","<<level; csv<<","<<spread<<"\n";
 }
 check(csv.good(),"level comparison CSV"); std::cout<<"Five-mode representative RMS spread: "<<worst<<" dB\n";
}
void hostAndCompatibility(const char* path) {
 juce::AudioPluginFormatManager manager; manager.addFormat(std::make_unique<juce::VST3PluginFormat>()); juce::String error;
 auto load=[&](const juce::File& file) { juce::OwnedArray<juce::PluginDescription> d; manager.getFormat(0)->findAllTypesForFile(d,file.getFullPathName()); check(d.size()==1,"Phase 3 hosted discovery"); return manager.createPluginInstance(*d[0],48000,128,error); };
 auto fixture=root().getChildFile("Tests/Fixtures/phase2-state.bin");
 if(!fixture.existsAsFile()) { auto old=load(root().getChildFile("build/Phase2Baseline/DISRUPTION.vst3")); check(old!=nullptr,"Phase 2 baseline load"); const float values[]={0.25f,0.5f,0.4f,0.6f,0.2f,0.8f,0.3f,0.5f,0,0}; for(int i=0;i<10;++i) old->getParameters()[i]->setValueNotifyingHost(values[i]); juce::MemoryBlock state; old->getStateInformation(state); check(fixture.replaceWithData(state.getData(),state.getSize()),"Phase 2 fixture save"); }
 juce::MemoryBlock state; check(fixture.loadFileAsData(state),"Phase 2 fixture read"); auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize())); check(xml && xml->getChildByName("IComponent"),"Phase 2 envelope");
 juce::MemoryBlock component; check(component.fromBase64Encoding(xml->getChildByName("IComponent")->getAllSubText()),"Phase 2 component decode"); DisruptionProcessor direct; direct.setStateInformation(component.getData(),static_cast<int>(component.getSize()));
 const char* ids[]={"mode","drive","damage","tone","body","crush","mix","output","hq","bypass"}; const float expected[]={1,18,40,20,20,80,30,-6,0,0};
 for(int i=0;i<10;++i) check(std::abs(direct.parameters.getRawParameterValue(ids[i])->load()-expected[i])<0.011,"Phase 2 component compatibility");
 auto current=load(juce::File(path)); check(current!=nullptr,"Phase 3 VST3 load"); current->setStateInformation(state.getData(),static_cast<int>(state.getSize())); for(int i=0;i<10;++i) check(std::abs(current->getParameters()[i]->getValue()-direct.getParameters()[i]->getValue())<1e-5,"Phase 2 hosted compatibility");
 current->getParameters()[3]->setValueNotifyingHost(0.5f); current->getParameters()[4]->setValueNotifyingHost(0); current->getParameters()[5]->setValueNotifyingHost(0); current->getParameters()[1]->setValueNotifyingHost(1); current->getParameters()[2]->setValueNotifyingHost(1); current->getParameters()[6]->setValueNotifyingHost(1); current->getParameters()[7]->setValueNotifyingHost(2.0f/3.0f);
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) for(int n:{1,2}) for(int mode:{3,4}) {
  channels(*current,n); current->getParameters()[0]->setValueNotifyingHost(mode/4.0f); current->prepareToPlay(rate,128); juce::AudioBuffer<float> b(n,128); juce::MidiBuffer midi; float audible=0;
  for(int block=0;block<8;++block) { for(int ch=0;ch<n;++ch) for(int i=0;i<128;++i) b.setSample(ch,i,signal(10,block*128+i,ch)); current->processBlock(b,midi); audible=std::max(audible,b.getMagnitude(0,128)); for(int ch=0;ch<n;++ch) for(int i=0;i<128;++i) check(std::isfinite(b.getSample(ch,i)) && std::abs(b.getSample(ch,i))<4,"hosted new engine invalid"); } check(audible>0,"hosted new engine silent over complete capture window"); current->releaseResources();
 }
 // Original three engines must continue rendering like the archived Phase 2 DLL.
 auto baseline=root().getChildFile("build/Phase2Baseline/DISRUPTION.vst3");
 if(baseline.exists()) {
  auto old=load(baseline); check(old!=nullptr,"Phase 2 audio baseline load");
  double worst=0;
  for(int mode=0;mode<3;++mode) for(float damage:{0.0f,50.0f,100.0f}) {
   for(auto* p:{old.get(),current.get()}) { for(int i=0;i<10;++i) p->getParameters()[i]->setValueNotifyingHost(0); p->getParameters()[3]->setValueNotifyingHost(0.5f); p->getParameters()[0]->setValueNotifyingHost(mode/4.0f); p->getParameters()[1]->setValueNotifyingHost(0.5f); p->getParameters()[2]->setValueNotifyingHost(damage/100); p->getParameters()[6]->setValueNotifyingHost(1); p->getParameters()[7]->setValueNotifyingHost(2.0f/3.0f); channels(*p,2); p->prepareToPlay(48000,128); }
   juce::AudioBuffer<float> a(2,128),b(2,128); juce::MidiBuffer midi;
   std::array<std::array<float,256>,2> delay {}; int pos=0; const int latency=current->getLatencySamples();
   for(int block=0;block<64;++block) { for(int ch=0;ch<2;++ch) for(int i=0;i<128;++i) a.setSample(ch,i,signal(10,block*128+i,ch)); b.copyFrom(0,0,a,0,0,128); b.copyFrom(1,0,a,1,0,128); old->processBlock(a,midi); current->processBlock(b,midi); for(int ch=0;ch<2;++ch) for(int i=0;i<128;++i) { int index=(pos+i)%latency; float oldSample=delay[ch][index]; delay[ch][index]=a.getSample(ch,i); worst=std::max(worst,static_cast<double>(std::abs(oldSample-b.getSample(ch,i)))); } pos=(pos+128)%latency; }
   old->releaseResources(); current->releaseResources();
  }
  check(worst<1e-6,"BURN/BREAK/RIP Phase 2 audio regression"); std::cout<<"Phase 2 original-three hosted audio comparison: max difference "<<worst<<"\n";
 }
 std::cout<<"Phase 2 session state and actual CRUSH/CHAOS VST3 at every required rate: PASS\n";
}
void benchmark() {
 std::ofstream csv(docs().getChildFile("cpu-benchmark.csv").getFullPathName().toStdString()); csv<<"sample_rate,mode,channels,block_size,median_us_per_block,p95_us_per_block,one_core_percent\n";
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) for(int mode=0;mode<6;++mode) for(int size:{64,256}) {
  DisruptionProcessor p; set(p.parameters,"mode",static_cast<float>(mode%5)); set(p.parameters,"drive",18); set(p.parameters,"damage",75); p.prepareToPlay(rate,size); juce::AudioBuffer<float> input(2,size),b(2,size); juce::MidiBuffer midi; for(int ch=0;ch<2;++ch) for(int i=0;i<size;++i) input.setSample(ch,i,0.2f*static_cast<float>(std::sin(2*pi*i/128.0+ch*0.37))+0.08f*static_cast<float>(std::sin(2*pi*i/32.0)));
  std::vector<double> times; times.reserve(1000);
  for(int iteration=0;iteration<1100;++iteration) { if(mode==5) set(p.parameters,"mode",static_cast<float>(iteration%5)); for(int ch=0;ch<2;++ch) b.copyFrom(ch,0,input,ch,0,size); track=true; const auto start=std::chrono::steady_clock::now(); p.processBlock(b,midi); const auto end=std::chrono::steady_clock::now(); track=false; if(iteration>=100) times.push_back(std::chrono::duration<double,std::micro>(end-start).count()); }
  std::sort(times.begin(),times.end()); const double median=times[500]; csv<<rate<<","<<(mode<5 ? names[mode] : "MODE_FADE")<<",2,"<<size<<","<<median<<","<<times[950]<<","<<median*rate/size/10000<<"\n";
 }
 check(csv.good(),"five-mode CPU CSV");
}
void aliasingEvidence() {
 std::ofstream csv(docs().getChildFile("aliasing-evidence.csv").getFullPathName().toStdString()); csv<<"sample_rate,mode,input_frequency,folded_frequency,folded_amplitude\n";
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) {
  disruption::CrushEngine crush; crush.prepare(rate); double re=0,im=0;
  track=true;
  for(int i=0;i<static_cast<int>(rate);++i) {
   crush.beginFrame(100); const float x=0.8f*static_cast<float>(std::sin(2*pi*437*i/rate)); const double y=crush.process(x,0,100);
   re+=y*std::cos(2*pi*163*i/rate); im+=y*std::sin(2*pi*163*i/rate);
  }
  track=false; const double amplitude=2*std::hypot(re,im)/rate;
  check(std::isfinite(amplitude) && amplitude>0.3,"CRUSH intentional alias evidence"); csv<<rate<<",CRUSH,437,163,"<<amplitude<<"\n";
  re=0; im=0; constexpr int n=10000;
  track=true;
  for(int i=0;i<n;++i) { const double phase=2*pi*0.21*i; const double y=disruption::ChaosEngine::transfer(0.8f*static_cast<float>(std::sin(phase)),75); re+=y*std::cos(2*pi*0.37*i); im+=y*std::sin(2*pi*0.37*i); }
  track=false; const double folded=2*std::hypot(re,im)/n;
  check(std::isfinite(folded) && folded>0.001,"CHAOS alias evidence"); csv<<rate<<",CHAOS,"<<0.21*rate<<","<<0.37*rate<<","<<folded<<"\n";
 }
 check(csv.good(),"Phase 3 alias CSV");
}
void artifacts() {
 std::ofstream curves(docs().getChildFile("chaos-transfer-curves.csv").getFullPathName().toStdString()); curves<<"damage,input,output\n";
 for(float damage:{0.0f,25.0f,50.0f,75.0f,100.0f}) for(int i=-2048;i<=2048;++i) curves<<damage<<","<<i/512.0f<<","<<disruption::ChaosEngine::transfer(i/512.0f,damage)<<"\n"; check(curves.good(),"CHAOS curves");
 std::ofstream hold(docs().getChildFile("crush-sample-hold.csv").getFullPathName().toStdString()); hold<<"frame,input,damage,captured,output\n";
 for(float damage:{0.0f,50.0f,100.0f}) { disruption::CrushEngine e; e.prepare(48000); for(int i=0;i<2048;++i) { const float input=0.8f*static_cast<float>(std::sin(2*pi*437*i/48000)); e.beginFrame(damage); hold<<i<<","<<input<<","<<damage<<","<<e.didCaptureThisFrame()<<","<<e.process(input,0,damage)<<"\n"; } } check(hold.good(),"CRUSH hold CSV");
 // Use the same synthetic source as Phase 2 so the five render comparisons are fair.
 auto sourceFile=root().getChildFile("Docs/Phase2/Listening-Source.wav"); juce::WavAudioFormat format; auto reader=std::unique_ptr<juce::AudioFormatReader>(format.createReaderFor(sourceFile.createInputStream().release(),true)); check(reader!=nullptr,"Phase 2 listening source missing");
 const int frames=static_cast<int>(reader->lengthInSamples); juce::AudioBuffer<float> source(2,frames); check(reader->read(&source,0,frames,0,true,true),"source WAV read");
 auto write=[&](const juce::String& name,juce::AudioBuffer<float>& audio) { auto file=docs().getChildFile(name); auto stream=std::make_unique<juce::FileOutputStream>(file); check(stream->openedOk(),"listening file open"); stream->setPosition(0); stream->truncate(); std::unique_ptr<juce::OutputStream> out=std::move(stream); auto writer=format.createWriterFor(out,juce::AudioFormatWriterOptions{}.withSampleRate(reader->sampleRate).withNumChannels(2).withBitsPerSample(32).withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint)); check(writer && writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples()),"Phase 3 WAV write"); writer.reset();
  auto verify=std::unique_ptr<juce::AudioFormatReader>(format.createReaderFor(file.createInputStream().release(),true));
  check(verify && verify->usesFloatingPointData && verify->lengthInSamples==audio.getNumSamples(),"float WAV format verification");
  juce::Range<float> ranges[2]; verify->readMaxLevels(0,audio.getNumSamples(),ranges,2);
  float written=0; for(const auto& range:ranges) written=std::max(written,std::max(std::abs(range.getStart()),std::abs(range.getEnd())));
  check(std::abs(written-audio.getMagnitude(0,audio.getNumSamples()))<1e-6,"raw WAV clipped or altered");
 };
 write("Listening-Source.wav",source);
 std::ofstream levels(docs().getChildFile("listening-levels.csv").getFullPathName().toStdString()); levels<<"mode,drive,damage,raw_rms,raw_peak,offline_match_gain_db\n";
 juce::AudioBuffer<float> comparison(2,frames*5);
 for(int mode=0;mode<5;++mode) {
  disruption::Controls c; c.mode=mode; c.drive=12; c.damage=65; disruption::DisruptionEngine e; e.prepare(reader->sampleRate,c); juce::AudioBuffer<float> audio(source); run(e,audio,c); const int fade=static_cast<int>(reader->sampleRate*0.01); audio.applyGainRamp(0,fade,0,1); audio.applyGainRamp(frames-fade,fade,1,0); double energy=0; for(int ch=0;ch<2;++ch) for(int i=0;i<frames;++i) energy+=std::pow(audio.getSample(ch,i),2);
  const double rms=std::sqrt(energy/(2*frames)); const float peak=audio.getMagnitude(0,frames); check(rms>0,"listening render silent"); const float match=static_cast<float>(0.1/rms); levels<<names[mode]<<",12,65,"<<rms<<","<<peak<<","<<20*std::log10(match)<<"\n";
  write(juce::String("Listening-")+names[mode]+"-Raw.wav",audio); audio.applyGain(match); check(audio.getMagnitude(0,frames)<1,"matched render clipping"); write(juce::String("Listening-")+names[mode]+"-RMS-matched.wav",audio);
  for(int ch=0;ch<2;++ch) comparison.copyFrom(ch,mode*frames,audio,ch,0,frames);
 }
 check(levels.good(),"listening levels CSV"); write("Listening-Five-Engine-Comparison.wav",comparison);
}
}
int runPhase3Tests(const char* path) {
 const int start=allocations;
 extremeMatrix(); holdClockAndQuantization(); foldingAndDC(); distinctnessAndDeterminism(); modeAndAutomation(); levelComparison(); hostAndCompatibility(path); benchmark(); aliasingEvidence(); artifacts();
 check(allocations==start,"Phase 3 callback allocation"); std::cout<<"Phase 3: "<<count<<" assertions passed; callback allocations "<<allocations-start<<"\n"; return count;
}



