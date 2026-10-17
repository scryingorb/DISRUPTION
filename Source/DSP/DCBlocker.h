#pragma once
#include "ModeEngine.h"
#include <cmath>
namespace disruption {
// Same five-Hz, double-history topology as RIP, reusable for stateful engines.
class DCBlocker {
public:
 void prepare(double rate) noexcept {
  if(!std::isfinite(rate) || rate<=0) rate=44100.0;
  pole=std::exp(-juce::MathConstants<double>::twoPi*5.0/rate); gain=(1.0+pole)*0.5; reset();
 }
 void reset() noexcept { input.fill(0); output.fill(0); }
 float process(float x,int channel) noexcept {
  if(channel<0 || channel>=2) return 0;
  const auto index=static_cast<size_t>(channel);
  const double y=gain*(static_cast<double>(x)-input[index])+pole*output[index];
  input[index]=x; output[index]=std::abs(y)<1.0e-24 ? 0.0 : y;
  return static_cast<float>(output[index]);
 }
private:
 std::array<double,2> input {},output {};
 double pole=0.999, gain=0.9995;
};
}
