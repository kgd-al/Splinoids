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

DEFINE_PARAMETER(float, critterBaseSpeed, 2)

#undef CFILE
} // end of namespace config
