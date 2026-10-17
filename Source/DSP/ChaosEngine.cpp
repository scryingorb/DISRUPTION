#include "ChaosEngine.h"
#include "TransferSafety.h"
namespace disruption {
double ChaosEngine::fold(double sample,double threshold) noexcept {
 if(!std::isfinite(sample) || !std::isfinite(threshold) || threshold<=0) return 0;
 // Periodic reflection, always in [-threshold,+threshold]. Unlike iterative
 // reflection, runtime does not grow with the number of folds or overload.
 return threshold-std::abs(std::remainder(sample-threshold,4.0*threshold));
}
ChaosEngine::Coefficients ChaosEngine::coefficients(float damage) noexcept {
 const double d=unitDamage(damage); Coefficients c;
 c.threshold=1.65*std::exp(std::log(0.16/1.65)*std::pow(d,0.85));
 c.amount=0.04+0.96*std::pow(d,0.8);
 c.foldBias=0.03+0.30*d*d;
 c.shapeBias=0.025+0.22*d*d;
 c.intensity=1.0/0.85+1.5*d;
 c.foldOffset=c.foldBias*c.threshold; c.zeroFold=fold(c.foldOffset,c.threshold);
 c.centre=std::tanh(c.shapeBias); c.normalization=(0.85/(1.0+0.35*d*d))/(1.0-c.centre*c.centre);
 return c;
}
float ChaosEngine::shape(float sample,const Coefficients& c) noexcept {
 const double x=finiteInput(sample);
 // Soft preconditioning has unity slope, but bounds arbitrary input to +/-12
 // before folding so overload does not sweep millions of fold periods.
 const double conditioned=x/std::sqrt(1.0+(x/12.0)*(x/12.0));
 const double folded=0.8*(fold(conditioned+c.foldOffset,c.threshold)-c.zeroFold)/c.threshold;
 const double mixed=(1.0-c.amount)*conditioned+c.amount*folded;
 // One biased saturator provides asymmetric compression and soft bounding.
 // Subtract the zero-input bias. There is no random process or final limiter.
 return static_cast<float>(c.normalization*(std::tanh(c.intensity*mixed+c.shapeBias)-c.centre));
}
float ChaosEngine::transfer(float sample,float damage) noexcept { return shape(sample,coefficients(damage)); }
void ChaosEngine::beginFrame(float damage) noexcept {
 const float legal=std::isfinite(damage) ? juce::jlimit(0.0f,100.0f,damage) : 0.0f;
 if(legal!=lastDamage) { current=coefficients(legal); lastDamage=legal; }
}
float ChaosEngine::process(float sample,int channel,float) noexcept {
 if(channel<0 || channel>=2) return 0;
 return dc.process(shape(sample,current),channel);
}
}



