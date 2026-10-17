#pragma once
#include "ModeEngine.h"
#include "DCBlocker.h"
namespace disruption {
class ChaosEngine final : public ModeEngine {
public:
 void prepare(double rate) noexcept override { dc.prepare(rate); lastDamage=-1; }
 void reset() noexcept override { dc.reset(); lastDamage=-1; }
 void beginFrame(float damage) noexcept override;
 float process(float sample,int channel,float) noexcept override;
 static float transfer(float sample,float damage) noexcept;
 static double fold(double sample,double threshold) noexcept;
private:
 struct Coefficients { double threshold=1.65, amount=0.04, foldBias=0.03, shapeBias=0.025, intensity=1.0/0.85, centre=0, normalization=1, foldOffset=0, zeroFold=0; };
 Coefficients current;
 float lastDamage=-1;
 DCBlocker dc;
 static Coefficients coefficients(float) noexcept;
 static float shape(float,const Coefficients&) noexcept;
};
}

