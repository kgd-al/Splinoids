#ifndef SIMU_CONFIG_H
#define SIMU_CONFIG_H

#include "../genotype/critter.h"
#include "../genotype/environment.h"
#include "kgd/apt/core/ptreeconfig.h"

#include "box2d/b2_math.h"

namespace simu {

using P2D = b2Vec2;
using real = decltype(P2D::x);

std::ostream& operator<< (std::ostream &os, const P2D &p);
P2D fromPolar (real a, real l);

P2D operator* (const P2D &p, real v);

using Color = genotype::Color;

struct Critter;
struct Foodlet;
struct Obstacle;

enum class BodyType { CRITTER, PLANT, CORPSE, OBSTACLE };

struct b2BodyUserData {
  BodyType type;
  union {
    Critter *critter;
    Foodlet *foodlet;
    Obstacle *obstacle; // Always null
  } ptr;
};

}
namespace config {
struct PTree;

struct CONFIG_FILE(Simulation) {
  DECLARE_SUBCONFIG(genotype::Critter::config_t, configCritter)
  DECLARE_SUBCONFIG(genotype::Environment::config_t, configEnvironment)

  DECLARE_SUBCONFIG(PTree, configPhylogeny)

  DECLARE_PARAMETER(std::string, logFile)

  // Foodlets
  DECLARE_PARAMETER(float, plantMinRadius)
  DECLARE_PARAMETER(float, plantMaxRadius)
  DECLARE_PARAMETER(float, plantEnergyDensity)
  DECLARE_PARAMETER(float, decompositionRate)

  // Splinoid constants
  DECLARE_PARAMETER(float, healthToEnergyRatio)
  DECLARE_PARAMETER(float, critterBaseSpeed)
  DECLARE_PARAMETER(float, combatBaselineIntensity)

  // SPlinoid metabolic constants (per second, affected by clock speed)
  DECLARE_PARAMETER(float, baselineAgingSpeed)
  DECLARE_PARAMETER(float, baselineEnergyConsumption)
  DECLARE_PARAMETER(float, baselineRegenerationRate)
  DECLARE_PARAMETER(float, motorEnergyConsumption)
  DECLARE_PARAMETER(float, energyAbsorptionRate)

  // Durations of world and physics
  DECLARE_PARAMETER(uint, b2VelocityIter)
  DECLARE_PARAMETER(uint, b2PositionIter)
  DECLARE_PARAMETER(uint, ticksPerSecond)
  DECLARE_PARAMETER(uint, secondsPerDay)
  DECLARE_PARAMETER(uint, daysPerYear)

  // Other
  DECLARE_PARAMETER(uint, ssgaMinPopSize)
};

} // end of namespace config

#endif // SIMU_CONFIG_H
