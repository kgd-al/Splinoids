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

namespace config {
#define CFILE Simulation

DEFINE_SUBCONFIG(genotype::Critter::config_t, configCritter)
DEFINE_SUBCONFIG(genotype::Environment::config_t, configEnvironment)

DEFINE_SUBCONFIG(PTree, configPhylogeny)

DEFINE_PARAMETER(std::string, logFile, "stats.dat")

DEFINE_PARAMETER(float, plantMinRadius, .25)
DEFINE_PARAMETER(float, plantMaxRadius, 1)
DEFINE_PARAMETER(float, plantEnergyDensity, 4)
DEFINE_PARAMETER(float, decompositionRate, .1)

DEFINE_PARAMETER(float, healthToEnergyRatio, .05)

DEFINE_PARAMETER(float, critterBaseSpeed, 4)

DEFINE_PARAMETER(float, combatBaselineIntensity, .1)

DEFINE_PARAMETER(float, baselineAgingSpeed, .01)
DEFINE_PARAMETER(float, baselineEnergyConsumption, .01)
DEFINE_PARAMETER(float, baselineRegenerationRate, .1)
DEFINE_PARAMETER(float, motorEnergyConsumption, .0025)
DEFINE_PARAMETER(float, energyAbsorptionRate, .1)

DEFINE_PARAMETER(uint, b2VelocityIter, 8)
DEFINE_PARAMETER(uint, b2PositionIter, 3)
DEFINE_PARAMETER(uint, ticksPerSecond, 50)
DEFINE_PARAMETER(uint, secondsPerDay, 200)
DEFINE_PARAMETER(uint, daysPerYear, 1000)

DEFINE_PARAMETER(uint, ssgaMinPopSize, 100)

#undef CFILE
} // end of namespace config
