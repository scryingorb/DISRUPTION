#include "Phase2Tests.h"
#include "PluginProcessor.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <limits>
extern thread_local bool track;
extern int allocations;
namespace {
int count=0;
constexpr double pi=3.14159265358979323846;
const char* names[]={"BURN","BREAK","RIP"};
void check(bool valid,const char* message) { ++count; if(!valid) throw std::runtime_error(message); }
juce::File root() { return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getParentDirectory().getParentDirectory(); }
juce::File docs() { auto f=root().getChildFile("Docs/Phase2"); check(f.createDirectory().wasOk(),"create Phase 2 docs"); return f; }
void set(DisruptionProcessor& p,const char* id,float value) { auto* parameter=p.parameters.getParameter(id); parameter->setValueNotifyingHost(parameter->convertTo0to1(value)); }
void channels(juce::AudioProcessor& p,int n) { juce::AudioProcessor::BusesLayout l; const auto s=n==1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo(); l.inputBuses.add(s); l.outputBuses.add(s); check(p.setBusesLayout(l),"Phase 2 bus layout"); }
void run(DisruptionProcessor& p,juce::AudioBuffer<float>& b) { juce::MidiBuffer midi; track=true; p.processBlock(b,midi); track=false; }
void run(disruption::DisruptionEngine& e,juce::AudioBuffer<float>& b,const disruption::Controls& c) { track=true; e.process(b,c); track=false; }
float raw(int mode,float x,float damage) {
 if(mode==0) return disruption::BurnEngine::transfer(x,damage);
 if(mode==1) return disruption::BreakEngine::transfer(x,damage);
 return disruption::RipEngine::transfer(x,damage);
}
float signal(int type,int frame,int channel) {
 const float s=static_cast<float>(std::sin(2*pi*frame/128.0+channel*0.37));
 switch(type) {
 case 0: return 0;
 case 1: return 1e-6f*s;
 case 2: return 0.2f*s+0.08f*static_cast<float>(std::sin(2*pi*frame/32.0));
 case 3: return s;
 case 4: return s>=0 ? 1.0f : -1.0f;
 case 5: return 1000.0f*s;
 case 6: return std::abs(s);
 case 7: return -std::abs(s);
 case 8: return frame==0 ? 1.0f : 0.0f;
 case 9: return 1.0f;
 case 10: return -1.0f;
 default: return (frame%3)==0 ? std::numeric_limits<float>::quiet_NaN() : ((frame%3)==1 ? std::numeric_limits<float>::infinity() : std::numeric_limits<float>::max());
 }
}
void extremeMatrix() {
 std::ofstream csv(docs().getChildFile("sample-rate-results.csv").getFullPathName().toStdString());
 csv<<"sample_rate,cases,peak,assertions,result\n";
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) {
  const int before=count; int cases=0; float peak=0;
  for(int n:{1,2}) for(int mode=0;mode<3;++mode) for(const auto setting: std::array<std::array<float,2>,7>{{ {0,0},{36,0},{0,100},{36,100},{6,25},{18,50},{30,75} }}) {
   for(int type=0;type<12;++type) {
    DisruptionProcessor p; channels(p,n); set(p,"mode",static_cast<float>(mode)); set(p,"drive",setting[0]); set(p,"damage",setting[1]); p.prepareToPlay(rate,128);
    check(p.getLatencySamples()>0,"mode added latency"); juce::AudioBuffer<float> b(n,128);
    for(int block=0;block<8;++block) {
     for(int c=0;c<n;++c) for(int i=0;i<128;++i) b.setSample(c,i,signal(type,block*128+i,c));
     run(p,b);
     for(int c=0;c<n;++c) for(int i=0;i<128;++i) {
      const float x=b.getSample(c,i); check(std::isfinite(x),"extreme matrix nonfinite"); check(std::abs(x)<4.0f,"real engine unbounded"); peak=std::max(peak,std::abs(x));
      if(type==0) check(x==0.0f,"real engine silence changed");
     }
    }
    ++cases;
   }
  }
  csv<<rate<<","<<cases<<","<<peak<<","<<count-before<<",PASS\n";
  std::cout<<"Extreme input matrix "<<rate<<" Hz: "<<cases<<" cases PASS, peak "<<peak<<"\n";
 }
 check(csv.good(),"sample rate CSV failed");
 // Engine entrypoints themselves must be safe, not only the enclosing processor.
 for(int mode=0;mode<3;++mode) for(float damage:{-1e20f,0.0f,100.0f,1e20f,std::numeric_limits<float>::quiet_NaN()}) for(float x:{-std::numeric_limits<float>::max(),-1e-30f,0.0f,1e-30f,std::numeric_limits<float>::max(),std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
  track=true; const float y=raw(mode,x,damage); track=false; check(std::isfinite(y) && std::abs(y)<2.0f,"standalone transfer invalid");
 }
}
void dcAndChannels() {
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) for(float damage:{0.0f,50.0f,100.0f}) {
  disruption::RipEngine rip; rip.prepare(rate); double tail=0; const int frames=static_cast<int>(rate*1.5);
  for(int i=0;i<frames;++i) { track=true; const float y=rip.process(1000.0f,0,damage); const float silent=rip.process(0,1,damage); track=false; check(silent==0,"RIP DC channel leak"); if(i>frames-1024) tail=std::max(tail,static_cast<double>(std::abs(y))); }
  check(tail<1e-7,"RIP DC did not settle"); rip.reset(); check(rip.process(0,0,damage)==0,"RIP reset history"); check(rip.process(1,3,damage)==0,"RIP invalid channel");
  for(int mode=0;mode<3;++mode) for(float dc:{-1000.0f,1.0f}) {
   disruption::DisruptionEngine engine; disruption::Controls c; c.mode=mode; c.damage=damage; c.drive=36; c.mix=50; engine.prepare(rate,c); juce::AudioBuffer<float> b(2,128);
   float maxTail=0; const int blocks=static_cast<int>(rate*1.5/128);
   for(int block=0;block<blocks;++block) { for(int i=0;i<128;++i) { b.setSample(0,i,dc); b.setSample(1,i,0); } run(engine,b,c); if(block>blocks-8) maxTail=std::max(maxTail,b.getMagnitude(0,0,128)); check(b.getMagnitude(1,0,128)==0,"DC stereo leak"); }
   check(maxTail<0.0003f,"full-chain DC did not settle");
  }
 }
}
void curvesAndDifference() {
 std::ofstream csv(docs().getChildFile("transfer-curves.csv").getFullPathName().toStdString()); csv<<"mode,damage,input,output\n";
 for(int mode=0;mode<3;++mode) for(float damage:{0.0f,25.0f,50.0f,75.0f,100.0f}) for(int i=-2048;i<=2048;++i) { const float x=i/512.0f; csv<<names[mode]<<","<<damage<<","<<x<<","<<raw(mode,x,damage)<<"\n"; }
 check(csv.good(),"transfer curve CSV failed");
 for(int mode=0;mode<3;++mode) {
  double damageDiff=0, driveDiff=0, independence=0;
  for(int i=0;i<4096;++i) { const float x=0.8f*static_cast<float>(std::sin(2*pi*i/4096));
   damageDiff+=std::pow(raw(mode,x*2,0)-raw(mode,x*2,100),2);
   driveDiff+=std::pow(raw(mode,x,60)-raw(mode,x*4,60),2);
   independence+=std::pow(raw(mode,x*4,0)-raw(mode,x,100),2);
  }
  check(std::sqrt(damageDiff/4096)>0.05,"DAMAGE has no meaningful effect"); check(std::sqrt(driveDiff/4096)>0.1,"DRIVE has no meaningful effect"); check(std::sqrt(independence/4096)>0.1,"DRIVE and DAMAGE redundant");
 }
 for(int a=0;a<3;++a) for(int b=a+1;b<3;++b) {
  double difference=0; for(int i=0;i<4096;++i) { float x=1.6f*static_cast<float>(std::sin(2*pi*i/4096)); difference+=std::pow(raw(a,x,70)-raw(b,x,70),2); }
  check(std::sqrt(difference/4096)>0.04,"engines null too closely");
 }
  // DAMAGE preserves small-signal gain while altering shape. This directly
 // distinguishes it from multiplying the DRIVE input gain by a DAMAGE factor.
 for(int mode=0;mode<3;++mode) {
  const float low=raw(mode,0.001f,0)/0.001f, high=raw(mode,0.001f,100)/0.001f;
  check(std::abs(low-high)<0.002f,"DAMAGE unexpectedly duplicates input gain");
 }
 check(std::abs(raw(0,1,0)+raw(0,-1,0))<1e-6,"low-DAMAGE BURN symmetry");
 check(std::abs(raw(0,1,100)+raw(0,-1,100))>0.08,"BURN DAMAGE character absent");
 check(std::abs(raw(1,0.8f*1.1f,0)/1.1f-raw(1,0.8f*0.68f,100)/0.68f)>0.05,"BREAK DAMAGE only scales threshold");
 check(std::abs(raw(2,4,0)/std::abs(raw(2,-4,0))-raw(2,4,100)/std::abs(raw(2,-4,100)))>0.4,"RIP DAMAGE asymmetry absent");
 std::ofstream harmonics(docs().getChildFile("harmonics.csv").getFullPathName().toStdString()); harmonics<<"mode,harmonic,amplitude_relative_to_fundamental\n";
 double ripEven=0, breakEven=0;
 for(int mode=0;mode<3;++mode) {
  double fundamental=0;
  for(int h=1;h<=10;++h) { double re=0,im=0;
   for(int i=0;i<9600;++i) { const double phase=2*pi*i/96.0; const float y=raw(mode,2.0f*static_cast<float>(std::sin(phase)),100); re+=y*std::cos(h*phase); im+=y*std::sin(h*phase); }
   const double amplitude=2*std::hypot(re,im)/9600; if(h==1) fundamental=amplitude;
   harmonics<<names[mode]<<","<<h<<","<<amplitude/fundamental<<"\n";
   if(h==2 && mode==2) ripEven=amplitude/fundamental;
   if(h==2 && mode==1) breakEven=amplitude/fundamental;
  }
 }
 check(ripEven>0.1 && breakEven<1e-5,"RIP even-harmonic identity missing"); check(harmonics.good(),"harmonics CSV failed");
 // C1 joins are tested at the actual BREAK knee boundaries; intentional flattened
 // peaks are valid, discontinuous value or slope joins are not.
 for(float damage:{0.0f,25.0f,50.0f,75.0f,100.0f}) {
  const float d=damage/100, t=1.1f-0.42f*d, w=t*(0.8f*(1-d)*(1-d)+0.01f);
  for(float join:{t-w,t+w}) { constexpr float step=0.0001f;
   const float left=raw(1,join-step,damage), centre=raw(1,join,damage), right=raw(1,join+step,damage);
   check(std::abs(right-left)<0.0003f,"BREAK knee discontinuity"); check(std::abs((right-centre)-(centre-left))/step<0.03f,"BREAK knee slope discontinuity");
  }
 }
 for(int mode=0;mode<3;++mode) { check(std::abs(raw(mode,0,100))<1e-7,"curve zero crossing"); check(std::abs(raw(mode,1e-5f,100)+raw(mode,-1e-5f,100))<1e-7,"curve jump at origin"); }
}
void automation() {
 std::ofstream csv(docs().getChildFile("automation-boundaries.csv").getFullPathName().toStdString()); csv<<"rate,initial_mode,control,first_sample_residual,max_residual_step\n";
 const char* controls[]={"drive","damage","mode","mix","output","bypass","mix_endpoint"};
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) for(int mode=0;mode<5;++mode) for(int action=0;action<7;++action) {
  disruption::Controls c; c.mode=mode; c.drive=12; c.damage=10; if(action==6) c.mix=0.1f;
  disruption::DisruptionEngine reference,changed; reference.prepare(rate,c); changed.prepare(rate,c); juce::AudioBuffer<float> a(2,64),b(2,64);
  auto fill=[&] { for(int ch=0;ch<2;++ch) for(int i=0;i<64;++i) { a.setSample(ch,i,0.25f); b.setSample(ch,i,0.25f); } };
  for(int block=0;block<100;++block) { fill(); run(reference,a,c); run(changed,b,c); }
  auto next=c; switch(action) { case 0: next.drive=36; break; case 1: next.damage=100; break; case 2: next.mode=(mode+1)%5; break; case 3: next.mix=0; break; case 4: next.output=12; break; case 5: next.bypass=true; break; default: next.mix=0; }
  double previous=0,first=0,maxStep=0;
  const int blocks=static_cast<int>(std::ceil(rate*0.03/64));
  for(int block=0;block<blocks;++block) { fill(); run(reference,a,c); run(changed,b,next);
   for(int i=0;i<64;++i) { const double residual=b.getSample(0,i)-a.getSample(0,i); if(block==0 && i==0) first=std::abs(residual); maxStep=std::max(maxStep,std::abs(residual-previous)); previous=residual; }
  }
  // Compare changed/reference residuals on a plateau, not waveform sample deltas.
  // Natural clipping is intentionally absent from this boundary test input.
  check(first<0.04,"automation first-sample discontinuity"); check(maxStep<0.06,"automation residual discontinuity");
  csv<<rate<<","<<mode<<","<<controls[action]<<","<<first<<","<<maxStep<<"\n";
 }
 check(csv.good(),"automation CSV failed");
 // Abrupt/repeated retargeting during active fades across all modes and controls.
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) {
  DisruptionProcessor p; p.prepareToPlay(rate,16); juce::AudioBuffer<float> b(2,16);
  for(int block=0;block<3000;++block) { set(p,"mode",static_cast<float>(block%5)); set(p,"drive",block%2 ? 36.0f : 0.0f); set(p,"damage",block%3 ? 100.0f : 0.0f); set(p,"mix",block%7 ? 100.0f : 0.0f); set(p,"output",block%11 ? 12.0f : -24.0f); set(p,"bypass",block%13 ? 0.0f : 1.0f);
   for(int ch=0;ch<2;++ch) for(int i=0;i<16;++i) b.setSample(ch,i,signal(2,block*16+i,ch)); run(p,b);
   for(int ch=0;ch<2;++ch) for(int i=0;i<16;++i) check(std::isfinite(b.getSample(ch,i)) && std::abs(b.getSample(ch,i))<80.0f,"automation stress unstable");
  }
 }
 // Per-mode parameter smoothing settles at the requested wall-clock duration.
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) {
  disruption::Controls initial; initial.mode=0; initial.damage=0; disruption::Controls target=initial; target.damage=100;
  disruption::DisruptionEngine actual,settled; actual.prepare(rate,initial); settled.prepare(rate,target);
  juce::AudioBuffer<float> a(1,1),b(1,1);
  for(int i=0;i<static_cast<int>(rate*0.02);++i) { a.setSample(0,0,0); b.setSample(0,0,0); run(actual,a,target); run(settled,b,target); }
  a.setSample(0,0,0.75f); b.setSample(0,0,0.75f); run(actual,a,target); run(settled,b,target); check(std::abs(a.getSample(0,0)-b.getSample(0,0))<1e-6,"DAMAGE smoothing did not settle");
 }
}
void levelMeasurements() {
 std::ofstream csv(docs().getChildFile("level-comparison.csv").getFullPathName().toStdString()); csv<<"source,amplitude,drive,damage,burn_rms,break_rms,rip_rms,spread_db\n"; double worst=0;
 for(int source:{0,1}) for(float amplitude:{0.1f,0.5f,1.0f}) for(float drive:{0.0f,12.0f,36.0f}) for(float damage:{0.0f,50.0f,100.0f}) {
  std::array<double,3> levels {};
  for(int mode=0;mode<3;++mode) {
   disruption::Controls c; c.mode=mode; c.drive=drive; c.damage=damage; disruption::DisruptionEngine e; e.prepare(48000,c); juce::AudioBuffer<float> b(1,128);
   double energy=0; int samples=0;
   for(int block=0;block<240;++block) { for(int i=0;i<128;++i) { const double phase=2*pi*(block*128+i)/128.0; const float x=source==0 ? static_cast<float>(std::sin(phase)) : static_cast<float>(0.6*std::sin(phase)+0.25*std::sin(3*phase)+0.15*std::sin(7*phase)); b.setSample(0,i,amplitude*x); } run(e,b,c); if(block>=120) { for(int i=0;i<128;++i) energy+=std::pow(b.getSample(0,i),2); samples+=128; } }
   levels[static_cast<size_t>(mode)]=std::sqrt(energy/samples);
  }
  const double spread=20*std::log10(*std::max_element(levels.begin(),levels.end()) / *std::min_element(levels.begin(),levels.end())); worst=std::max(worst,spread);
  check(spread<6.0,"comparable mode levels exceed 6 dB spread"); csv<<(source==0 ? "sine" : "multisine")<<","<<amplitude<<","<<drive<<","<<damage<<","<<levels[0]<<","<<levels[1]<<","<<levels[2]<<","<<spread<<"\n";
 }
 check(csv.good(),"level CSV failed"); std::cout<<"Worst real-mode RMS spread (54 settings): "<<worst<<" dB\n";
}
void aliasingMeasurements() {
 std::ofstream csv(docs().getChildFile("aliasing-evidence.csv").getFullPathName().toStdString()); csv<<"sample_rate,mode,input_frequency,folded_third_frequency,folded_amplitude_relative_to_fundamental\n";
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) for(int mode=0;mode<3;++mode) {
  // f0=.21*fs: the third harmonic at .63*fs folds to .37*fs.
  // Integer-cycle DFT avoids attributing spectral leakage to aliasing.
  double fundamentalRe=0,fundamentalIm=0,foldedRe=0,foldedIm=0;
  constexpr int n=10000;
  for(int i=0;i<n;++i) {
   const double phase=2*pi*0.21*i; const double y=raw(mode,4*static_cast<float>(std::sin(phase)),100);
   fundamentalRe+=y*std::cos(phase); fundamentalIm+=y*std::sin(phase);
   foldedRe+=y*std::cos(2*pi*0.37*i); foldedIm+=y*std::sin(2*pi*0.37*i);
  }
  const double ratio=std::hypot(foldedRe,foldedIm)/std::hypot(fundamentalRe,fundamentalIm);
  check(std::isfinite(ratio) && ratio>0.01,"aliasing evidence measurement invalid");
  csv<<rate<<","<<names[mode]<<","<<rate*0.21<<","<<rate*0.37<<","<<ratio<<"\n";
 }
 check(csv.good(),"aliasing evidence CSV failed");
}
void phase1Compatibility(const char* vst3Path) {
 juce::AudioPluginFormatManager manager; manager.addFormat(std::make_unique<juce::VST3PluginFormat>());
 const auto fixture=root().getChildFile("Tests/Fixtures/phase1-state.bin");
 if(!fixture.existsAsFile()) {
  const auto baseline=root().getChildFile("build/Phase1Baseline/DISRUPTION.vst3"); check(baseline.exists(),"missing Phase 1 state fixture and baseline");
  juce::OwnedArray<juce::PluginDescription> descriptions; manager.getFormat(0)->findAllTypesForFile(descriptions,baseline.getFullPathName()); check(descriptions.size()==1,"baseline discovery"); juce::String error;
  auto old=manager.createPluginInstance(*descriptions[0],48000,128,error); check(old!=nullptr,"baseline load");
  const float values[]={0.5f,0.25f,0.75f,0.4f,0.35f,0.6f,0.5f,0.7f,1.0f,1.0f};
  for(int i=0;i<10;++i) old->getParameters()[i]->setValueNotifyingHost(values[i]);
  juce::MemoryBlock state; old->getStateInformation(state); check(fixture.replaceWithData(state.getData(),state.getSize()),"save actual Phase 1 state fixture");
 }
 juce::MemoryBlock state; check(fixture.loadFileAsData(state),"load Phase 1 fixture");
 // JUCE hosts wrap the component state in VST3PluginState. The direct
 // processor takes the embedded APVTS component bytes, not the host envelope.
 auto envelope=juce::AudioProcessor::getXmlFromBinary(state.getData(),static_cast<int>(state.getSize()));
 check(envelope!=nullptr && envelope->hasTagName("VST3PluginState"),"Phase 1 host envelope");
 auto* component=envelope->getChildByName("IComponent"); check(component!=nullptr,"Phase 1 component missing");
 juce::MemoryBlock componentState; check(componentState.fromBase64Encoding(component->getAllSubText()),"decode Phase 1 component");
 DisruptionProcessor current; current.setStateInformation(componentState.getData(),static_cast<int>(componentState.getSize()));
 const char* ids[]={"mode","drive","damage","tone","body","crush","mix","output","hq","bypass"};
 const float expected[]={2,9,75,-20,35,60,50,1.2f,1,1};
 for(int i=0;i<10;++i) check(std::abs(current.parameters.getRawParameterValue(ids[i])->load()-expected[i])<0.011f,"Phase 1 fixture incompatibility");
 juce::OwnedArray<juce::PluginDescription> descriptions; manager.getFormat(0)->findAllTypesForFile(descriptions,vst3Path); check(descriptions.size()==1,"Phase 2 VST3 discovery"); juce::String error;
 auto hosted=manager.createPluginInstance(*descriptions[0],48000,128,error); check(hosted!=nullptr,"Phase 2 hosted load"); hosted->setStateInformation(state.getData(),static_cast<int>(state.getSize()));
 for(int i=0;i<10;++i) check(std::abs(hosted->getParameters()[i]->getValue()-current.getParameters()[i]->getValue())<1e-5,"host Phase 1 fixture incompatibility");
  hosted->getParameters()[3]->setValueNotifyingHost(0.5f); hosted->getParameters()[4]->setValueNotifyingHost(0); hosted->getParameters()[5]->setValueNotifyingHost(0); hosted->getParameters()[8]->setValueNotifyingHost(0); hosted->getParameters()[9]->setValueNotifyingHost(0); // bypass off
 hosted->getParameters()[6]->setValueNotifyingHost(1); // fully wet
 hosted->getParameters()[7]->setValueNotifyingHost(2.0f/3.0f); // OUTPUT 0 dB
 hosted->getParameters()[1]->setValueNotifyingHost(1); // DRIVE +36 dB
 hosted->getParameters()[2]->setValueNotifyingHost(1); // DAMAGE 100%
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) for(int n:{1,2}) for(int mode=0;mode<3;++mode) {
  channels(*hosted,n); hosted->getParameters()[0]->setValueNotifyingHost(mode/4.0f); hosted->prepareToPlay(rate,128);
  juce::AudioBuffer<float> b(n,128); juce::MidiBuffer midi;
  for(int block=0;block<8;++block) {
   for(int ch=0;ch<n;++ch) for(int i=0;i<128;++i) b.setSample(ch,i,signal(3,block*128+i,ch));
   hosted->processBlock(b,midi);
   check(b.getMagnitude(0,128)>0,"real hosted engine silent");
   for(int ch=0;ch<n;++ch) for(int i=0;i<128;++i) check(std::isfinite(b.getSample(ch,i)) && std::abs(b.getSample(ch,i))<4,"real hosted engine unstable");
  }
  hosted->releaseResources();
 }
 std::cout<<"Actual Phase 1 binary state -> Phase 2 processor and VST3: PASS\n";
 std::cout<<"Actual VST3 BURN/BREAK/RIP at all four sample rates, mono/stereo: PASS\n";
}
void benchmark() {
 std::ofstream csv(docs().getChildFile("cpu-benchmark.csv").getFullPathName().toStdString()); csv<<"sample_rate,mode,channels,block_size,median_us_per_block,p95_us_per_block,one_core_percent\n";
 for(double rate:{44100.0,48000.0,88200.0,96000.0}) for(int mode=0;mode<4;++mode) for(int size:{64,256}) {
  DisruptionProcessor p; set(p,"mode",static_cast<float>(mode%3)); set(p,"drive",18); set(p,"damage",75); p.prepareToPlay(rate,size); juce::AudioBuffer<float> input(2,size),b(2,size); juce::MidiBuffer midi;
  for(int ch=0;ch<2;++ch) for(int i=0;i<size;++i) input.setSample(ch,i,signal(2,i,ch));
  std::vector<double> times; times.reserve(1000);
  for(int iteration=0;iteration<1100;++iteration) {
   if(mode==3) set(p,"mode",static_cast<float>(iteration%3));
   for(int ch=0;ch<2;++ch) b.copyFrom(ch,0,input,ch,0,size);
   track=true; const auto start=std::chrono::steady_clock::now(); p.processBlock(b,midi); const auto stop=std::chrono::steady_clock::now(); track=false;
   if(iteration>=100) times.push_back(std::chrono::duration<double,std::micro>(stop-start).count());
  }
  std::sort(times.begin(),times.end()); const double median=times[500],p95=times[950];
  csv<<rate<<","<<(mode<3 ? names[mode] : "MODE_FADE")<<",2,"<<size<<","<<median<<","<<p95<<","<<median*rate/size/10000.0<<"\n";
 }
 check(csv.good(),"CPU CSV failed");
}
void listeningFiles() {
 constexpr int rate=48000, frames=rate*10;
 juce::AudioBuffer<float> source(2,frames);
 uint32_t rng=0x164abcdu;
 for(int i=0;i<frames;++i) {
  const double time=static_cast<double>(i)/rate; const int section=i/(rate*2); double x=0;
  if(section==0) { const double phase=2*pi*(140*time+0.1*std::sin(2*pi*5*time)); x=(0.65*std::sin(phase)+0.2*std::sin(2*phase)+0.1*std::sin(3*phase))* (0.65+0.35*std::sin(2*pi*2.5*time)); }
  else if(section==1) x=0.75*std::sin(2*pi*55*time)+0.15*std::sin(2*pi*110*time)+0.1*std::sin(2*pi*165*time);
  else if(section==2) { const double age=std::fmod(time,0.25); rng=1664525u*rng+1013904223u; const double noise=static_cast<double>(rng)/4294967295.0*2-1; x=0.65*std::sin(2*pi*70*age)*std::exp(-age*24)+0.3*noise*std::exp(-age*65); }
  else if(section==3) { for(int h=1;h<=12;++h) x+=0.4/h*std::sin(2*pi*110*h*time); }
  else x=0.4*std::sin(2*pi*110*time)+0.3*std::sin(2*pi*146.832*time)+0.3*std::sin(2*pi*164.814*time);
  const double fade=std::min({1.0,time*50.0,(10.0-time)*50.0});
  source.setSample(0,i,static_cast<float>(0.45*x*fade)); source.setSample(1,i,static_cast<float>(0.43*x*fade));
 }
 auto write=[&](const juce::String& name,juce::AudioBuffer<float>& audio) {
  const auto file=docs().getChildFile(name); auto stream=std::make_unique<juce::FileOutputStream>(file); check(stream->openedOk(),"WAV open"); stream->setPosition(0); stream->truncate();
  std::unique_ptr<juce::OutputStream> out=std::move(stream); juce::WavAudioFormat format;
  auto writer=format.createWriterFor(out,juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(2).withBitsPerSample(24));
  check(writer!=nullptr && writer->writeFromAudioSampleBuffer(audio,0,frames),"WAV write");
 };
 write("Listening-Source.wav",source);
 std::ofstream csv(docs().getChildFile("listening-levels.csv").getFullPathName().toStdString()); csv<<"mode,raw_rms,raw_peak,offline_match_gain_db\n";
 for(int mode=0;mode<3;++mode) {
  juce::AudioBuffer<float> rendered(source); disruption::DisruptionEngine engine; disruption::Controls c; c.mode=mode; c.drive=12; c.damage=65; engine.prepare(rate,c);
  run(engine,rendered,c); double sum=0; for(int ch=0;ch<2;++ch) for(int i=0;i<frames;++i) sum+=std::pow(rendered.getSample(ch,i),2);
  const double rms=std::sqrt(sum/(2*frames)); const float peak=rendered.getMagnitude(0,frames); const float compensation=static_cast<float>(0.1/rms);
  csv<<names[mode]<<","<<rms<<","<<peak<<","<<20*std::log10(compensation)<<"\n";
  rendered.applyGain(compensation); check(rendered.getMagnitude(0,frames)<1,"listening WAV clipping"); write(juce::String("Listening-")+names[mode]+"-RMS-matched.wav",rendered);
 }
 check(csv.good(),"listening CSV failed");
}
}
int runPhase2Tests(const char* path) {
 const int allocationStart=allocations;
 extremeMatrix(); dcAndChannels(); curvesAndDifference(); automation(); levelMeasurements(); aliasingMeasurements(); phase1Compatibility(path); benchmark(); listeningFiles();
 check(allocations==allocationStart,"Phase 2 callback allocations");
 std::cout<<"Phase 2: "<<count<<" assertions passed; callback allocations "<<allocations-allocationStart<<"\n";
 return count;
}





