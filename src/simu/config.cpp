#include "config.h"

namespace simu {

std::ostream& operator<< (std::ostream &os, const P2D &p) {
  return os << "{ " << p.x << ", " << p.y << " }";
}

P2D fromPolar(real a, real l) {
  return l * P2D(std::cos(a), std::sin(a));
}

P2D operator* (const P2D &p, real v) {
  return v * p;
}

} // end of namespace simu

//namespace cnl {

//void from_json (const nlohmann::json &j, simu::decimal &f) {
//  f = j.get<float>();
//}

//void to_json (nlohmann::json &j, const simu::decimal &f) {
//  j = float(f);
//}

//} // end of namespace cnl

namespace config {
#define CFILE Simulation
using decimal = CFILE::decimal;

DEFINE_SUBCONFIG(genotype::Critter::config_t, configCritter)
DEFINE_SUBCONFIG(genotype::Environment::config_t, configEnvironment)

DEFINE_SUBCONFIG(PTree, configPhylogeny)

DEFINE_PARAMETER(std::string, logFile, "stats.dat")

DEFINE_PARAMETER(Color, emptyColor, utils::uniformStdArray<Color>(1))
DEFINE_PARAMETER(Color, obstacleColor, utils::uniformStdArray<Color>(0))

DEFINE_PARAMETER(float, plantMinRadius, .75)
DEFINE_PARAMETER(float, plantMaxRadius, 1.25)
DEFINE_PARAMETER(float, plantEnergyDensity, 2)
DEFINE_PARAMETER(decimal, decompositionRate, .01)

DEFINE_PARAMETER(int, growthSubsteps, 4)
DEFINE_PARAMETER(decimal, healthToEnergyRatio, .05)
DEFINE_PARAMETER(float, critterBaseSpeed, 1)
DEFINE_PARAMETER(float, combatBaselineIntensity, .1)
DEFINE_PARAMETER(float, combatMinVelocity, 1.)
DEFINE_PARAMETER(float, reproductionRange, 3)
DEFINE_PARAMETER(float, reproductionRequestThreshold, .9)

DEFINE_PARAMETER(float, baselineAgingSpeed, .001)
DEFINE_PARAMETER(decimal, baselineEnergyConsumption, .0005)
DEFINE_PARAMETER(decimal, baselineRegenerationRate, .01)
DEFINE_PARAMETER(decimal, baselineGametesGrowth, .002)
DEFINE_PARAMETER(decimal, motorEnergyConsumption, .00025)
DEFINE_PARAMETER(decimal, energyAbsorptionRate, .025)

DEFINE_PARAMETER(bool, b2FixedBodyCOM, true)
DEFINE_PARAMETER(uint, b2VelocityIter, 8)
DEFINE_PARAMETER(uint, b2PositionIter, 3)
DEFINE_PARAMETER(uint, ticksPerSecond, 10)
DEFINE_PARAMETER(uint, secondsPerDay, 100)
DEFINE_PARAMETER(uint, daysPerYear, 1000)

DEFINE_PARAMETER(bool, screwTheEntropy, true)
DEFINE_PARAMETER(uint, ssgaMinPopSizeRatio, 1)
DEFINE_PARAMETER(uint, ssgaArchiveSizeRatio, 2)
DEFINE_PARAMETER(uint, ssgaMutationProbability, .5)
DEFINE_PARAMETER(uint, ssgaMaxGenerationalSpan, 10)

#undef CFILE
} // end of namespace config
