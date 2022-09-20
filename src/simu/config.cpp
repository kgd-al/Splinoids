#include "config.h"

namespace simu {

std::ostream& operator<< (std::ostream &os, const P2D &p) {
  return os << "{ " << p.x << ", " << p.y << " }";
}

P2D fromPolar(real a, real l) {
  return l * P2D(std::cos(a), std::sin(a));
// To help debug differences between local glibc2.30+ et olympe's glibc2.17
//  auto p = l * P2D(std::cos(a), std::sin(a));
//  std::cerr << std::setprecision(20) << p << "\n"
//            << "=\n"
//            << l << " * " << P2D(std::cos(a), std::sin(a)) << "\n"
//            << l << " * P2D(cos(" << a << "), sin(" << a << "))\n";
//  return p;
}

P2D operator* (const P2D &p, real v) {
  return v * p;
}

} // end of namespace simu

void assertEqual (const b2Vec2 &lhs, const b2Vec2 &rhs, bool deepcopy) {
  using utils::assertEqual;
  assertEqual(lhs.x, rhs.x, deepcopy);
  assertEqual(lhs.y, rhs.y, deepcopy);
}

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
DEFINE_PARAMETER(uint, logStatsEvery, 0)
DEFINE_PARAMETER(int, verbosity, 1)

DEFINE_PARAMETER(Color, emptyColor, utils::uniformStdArray<Color>(0))
DEFINE_PARAMETER(Color, obstacleColor, Color{{0.f, 0.f, 1.f}})

DEFINE_PARAMETER(float, plantMinRadius, .25)
DEFINE_PARAMETER(float, plantMaxRadius, .75)
DEFINE_PARAMETER(float, plantEnergyDensity, 4)
DEFINE_PARAMETER(decimal, decompositionRate, .01)

DEFINE_PARAMETER(int, growthSubsteps, 4)
DEFINE_PARAMETER(float, visionWidthToLength, 10)
DEFINE_PARAMETER(decimal, healthToEnergyRatio, .05)
DEFINE_PARAMETER(float, critterBaseSpeed, 1)
DEFINE_PARAMETER(float, critterArmSpeed, 1)
DEFINE_PARAMETER(float, combatBaselineIntensity, .25)
DEFINE_PARAMETER(float, combatMinImpulse, .25)
DEFINE_PARAMETER(float, combatMinVelocity, .25)
DEFINE_PARAMETER(float, auditionRange, 20)
DEFINE_PARAMETER(float, reproductionRange, 3)
DEFINE_PARAMETER(float, reproductionRequestThreshold, .9)

DEFINE_PARAMETER(float, baselineAgingSpeed, .001)
DEFINE_PARAMETER(decimal, baselineEnergyConsumption, .0005)
DEFINE_PARAMETER(decimal, motorEnergyConsumption, .00025)
DEFINE_PARAMETER(decimal, voiceEnergyConsumption, .0005)
DEFINE_PARAMETER(decimal, armEnergyConsumption, .000125)
DEFINE_PARAMETER(decimal, neuronEnergyConsumption, .00005)
DEFINE_PARAMETER(decimal, axonEnergyCost, .0001)
DEFINE_PARAMETER(decimal, baselineRegenerationRate, .01)
DEFINE_PARAMETER(decimal, baselineGametesGrowth, .001)
DEFINE_PARAMETER(decimal, energyAbsorptionRate, .025)

DEFINE_PARAMETER(bool, b2FixedBodyCOM, true)
DEFINE_PARAMETER(uint, b2VelocityIter, 8)
DEFINE_PARAMETER(uint, b2PositionIter, 3)
DEFINE_PARAMETER(uint, ticksPerSecond, 10)
DEFINE_PARAMETER(uint, secondsPerDay, 100)
DEFINE_PARAMETER(uint, daysPerYear, 1000)
DEFINE_PARAMETER(float, soundAttenuation, 2.f/CFILE::auditionRange())
DEFINE_PARAMETER(bool, selfHearing, true)

DEFINE_PARAMETER(bool, screwTheEntropy, true)
DEFINE_PARAMETER(uint, ssgaMinPopSizeRatio, 1)
DEFINE_PARAMETER(uint, ssgaArchiveSizeRatio, 0)
DEFINE_PARAMETER(uint, ssgaMutationProbability, .5)
DEFINE_PARAMETER(uint, ssgaMaxGenerationalSpan, 5)

#undef CFILE
} // end of namespace config
