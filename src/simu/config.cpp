#include "config.h"

namespace config {
#define CFILE Simulation

DEFINE_SUBCONFIG(genotype::Critter::config_t, configCritter)
DEFINE_SUBCONFIG(genotype::Environment::config_t, configEnvironment)

DEFINE_SUBCONFIG(PTree, configPhylogeny)

#undef CFILE
} // end of namespace config
