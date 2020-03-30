#ifndef SIMU_CONFIG_H
#define SIMU_CONFIG_H

#include "../genotype/critter.h"
#include "../genotype/environment.h"
#include "kgd/apt/core/ptreeconfig.h"

#include "box2d/b2_math.h"

namespace simu {

using decimal = double;

using P2D = b2Vec2;
using real = decltype(P2D::x);

std::ostream& operator<< (std::ostream &os, const P2D &p);
P2D fromPolar (real a, real l);

P2D operator* (const P2D &p, real v);

using Color = genotype::Color;

struct Critter;
struct Foodlet;
struct Obstacle;

enum class BodyType : uint8 {
  CRITTER,
  CORPSE,
  PLANT,
  OBSTACLE,
  WARP_ZONE
};

struct b2BodyUserData {
  BodyType type;
  union {
    Critter *critter;
    Foodlet *foodlet;
    Obstacle *obstacle; // Always null
  } ptr;
};

enum struct CollisionFlag : uint16 {
  OBSTACLE_FLAG = 0x01,
  CRITTER_BODY_FLAG = 0x02,
  CRITTER_SPLN_FLAG = 0x04,
  PLANT_FLAG = 0x08,
  CORPSE_FLAG = 0x10,

  ALL_MASK = 0x1F,
  OBSTACLE_MASK = ALL_MASK,
  CRITTER_BODY_MASK = ALL_MASK,
  CRITTER_SPLN_MASK = ALL_MASK ^ CORPSE_FLAG,
  PLANT_MASK = ALL_MASK,
  CORPSE_MASK = ALL_MASK,
};

}
namespace config {
struct PTree;

struct CONFIG_FILE(Simulation) {
  using decimal = simu::decimal;

  static constexpr float epsilonE = 1e-6;

  DECLARE_SUBCONFIG(genotype::Critter::config_t, configCritter)
  DECLARE_SUBCONFIG(genotype::Environment::config_t, configEnvironment)

  DECLARE_SUBCONFIG(PTree, configPhylogeny)

  DECLARE_PARAMETER(std::string, logFile)

  DECLARE_PARAMETER(Color, emptyColor)
  DECLARE_PARAMETER(Color, obstacleColor)

  // Foodlets
  DECLARE_PARAMETER(float, plantMinRadius)
  DECLARE_PARAMETER(float, plantMaxRadius)
  DECLARE_PARAMETER(float, plantEnergyDensity)
  DECLARE_PARAMETER(decimal, decompositionRate)

  // Splinoid constants
  DECLARE_PARAMETER(int, growthSubsteps)
  DECLARE_PARAMETER(decimal, healthToEnergyRatio)
  DECLARE_PARAMETER(float, critterBaseSpeed)
  DECLARE_PARAMETER(float, combatBaselineIntensity)

  // Splinoid metabolic constants (per second, affected by clock speed)
  DECLARE_PARAMETER(float, baselineAgingSpeed)
  DECLARE_PARAMETER(decimal, baselineEnergyConsumption)
  DECLARE_PARAMETER(decimal, baselineRegenerationRate)
  DECLARE_PARAMETER(decimal, motorEnergyConsumption)
  DECLARE_PARAMETER(decimal, energyAbsorptionRate)

  // Durations of world and physics
  DECLARE_PARAMETER(bool, b2FixedBodyCOM)
  DECLARE_PARAMETER(uint, b2VelocityIter)
  DECLARE_PARAMETER(uint, b2PositionIter)
  DECLARE_PARAMETER(uint, ticksPerSecond)
  DECLARE_PARAMETER(uint, secondsPerDay)
  DECLARE_PARAMETER(uint, daysPerYear)

  // Other
  DECLARE_PARAMETER(bool, screwTheEntropy)
  DECLARE_PARAMETER(uint, ssgaMinPopSizeRatio)  // Of the initial population size
};

} // end of namespace config

#endif // SIMU_CONFIG_H
