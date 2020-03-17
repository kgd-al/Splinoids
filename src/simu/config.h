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

}
namespace config {
struct PTree;

struct CONFIG_FILE(Simulation) {
  DECLARE_SUBCONFIG(genotype::Critter::config_t, configCritter)
  DECLARE_SUBCONFIG(genotype::Environment::config_t, configEnvironment)

  DECLARE_SUBCONFIG(PTree, configPhylogeny)

  DECLARE_PARAMETER(float, critterBaseSpeed)

  DECLARE_PARAMETER(uint, b2VelocityIter)
  DECLARE_PARAMETER(uint, b2PositionIter)
  DECLARE_PARAMETER(uint, ticksPerSecond)
  DECLARE_PARAMETER(uint, secondsPerDay)
  DECLARE_PARAMETER(uint, daysPerYear)
};

} // end of namespace config

#endif // SIMU_CONFIG_H
