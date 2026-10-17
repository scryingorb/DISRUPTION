#include "CrushEngine.h"
#include "TransferSafety.h"
#include "DigitalQuantizer.h"
namespace disruption {
float CrushEngine::effectiveBits(float damage) noexcept {
 return 16.0f - 13.0f * std::pow(unitDamage(damage), 0.85f);
}
double CrushEngine::requestedSampleRate(float damage) noexcept {
 return 96000.0 * std::exp(std::log(600.0 / 96000.0) * std::pow(static_cast<double>(unitDamage(damage)), 1.4));
}
float CrushEngine::quantizeWithLevels(float sample,float lower,float upper,float fraction) noexcept {
 return quantizeLevels(sample,lower,upper,fraction);
}
float CrushEngine::quantize(float sample,float damage) noexcept {
 const float bits=effectiveBits(damage); const int integer=static_cast<int>(std::floor(bits));
 const float lower=std::ldexp(1.0f,integer-1);
 return quantizeWithLevels(sample,lower,integer<16 ? lower*2.0f : lower,bits-static_cast<float>(integer));
}
void CrushEngine::prepare(double sampleRate) noexcept {
 rate=std::isfinite(sampleRate) && sampleRate>0.0 ? sampleRate : 44100.0; reset();
}
void CrushEngine::reset() noexcept { phase=0; held.fill(0); firstFrame=true; capture=false; lastDamage=-1; }
void CrushEngine::beginFrame(float damage) noexcept {
 const float d=std::isfinite(damage) ? juce::jlimit(0.0f,100.0f,damage) : 0.0f;
 if(d!=lastDamage) {
  lastDamage=d; targetRate=std::min(rate,requestedSampleRate(d));
  const float bits=effectiveBits(d); const int integer=static_cast<int>(std::floor(bits));
  lowerLevels=std::ldexp(1.0f,integer-1); upperLevels=integer<16 ? lowerLevels*2.0f : lowerLevels;
  bitFraction=bits-static_cast<float>(integer);
 }
 capture=firstFrame;
 if(firstFrame) { firstFrame=false; return; }
 phase+=targetRate/rate;
 if(phase>=1.0-1.0e-12) { capture=true; phase=std::max(0.0,phase-1.0); }
}
float CrushEngine::process(float sample,int channel,float) noexcept {
 if(channel<0 || channel>=2) return 0;
 const auto index=static_cast<size_t>(channel);
 if(capture) held[index]=juce::jlimit(-1.0f,1.0f,finiteInput(sample));
 // Quantize the held input with continuously blended adjacent integer grids.
 // Coefficient automation remains smooth while actual quantization/hold steps
 // remain fully present. No hold-phase resets on parameter or mode changes.
 return quantizeWithLevels(held[index],lowerLevels,upperLevels,bitFraction);
}
}

