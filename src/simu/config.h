#ifndef SIMU_CONFIG_H
#define SIMU_CONFIG_H

#include "../genotype/critter.h"
#include "../genotype/environment.h"
#include "kgd/apt/core/ptreeconfig.h"

namespace config {
struct PTree;

struct CONFIG_FILE(Simulation) {
  DECLARE_SUBCONFIG(genotype::Critter::config_t, configCritter)
  DECLARE_SUBCONFIG(genotype::Environment::config_t, configEnvironment)

  DECLARE_SUBCONFIG(PTree, configPhylogeny)

  static constexpr float GRID_SUBDIVISION = 2;
};

} // end of namespace config

#endif // SIMU_CONFIG_H
