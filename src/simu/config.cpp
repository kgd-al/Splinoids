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

DEFINE_PARAMETER(float, plantMinRadius, 1)//.25) TODO Reset after implementing growth
// (plants must be smaller than critters for the ssga to have a change to work)

DEFINE_PARAMETER(float, plantMaxRadius, 1)
DEFINE_PARAMETER(float, plantEnergyDensity, 4)
DEFINE_PARAMETER(decimal, decompositionRate, .01)

DEFINE_PARAMETER(decimal, healthToEnergyRatio, .05)

DEFINE_PARAMETER(float, critterBaseSpeed, 4)

DEFINE_PARAMETER(float, combatBaselineIntensity, .1)

DEFINE_PARAMETER(float, baselineAgingSpeed, .01)
DEFINE_PARAMETER(decimal, baselineEnergyConsumption, .01)
DEFINE_PARAMETER(decimal, baselineRegenerationRate, .1)
DEFINE_PARAMETER(decimal, motorEnergyConsumption, .0025)
DEFINE_PARAMETER(decimal, energyAbsorptionRate, .1)

DEFINE_PARAMETER(uint, b2VelocityIter, 8)
DEFINE_PARAMETER(uint, b2PositionIter, 3)
DEFINE_PARAMETER(uint, ticksPerSecond, 50)
DEFINE_PARAMETER(uint, secondsPerDay, 200)
DEFINE_PARAMETER(uint, daysPerYear, 1000)

DEFINE_PARAMETER(bool, screwTheEntropy, true)
DEFINE_PARAMETER(uint, ssgaMinPopSize, 1)

#undef CFILE
} // end of namespace config
