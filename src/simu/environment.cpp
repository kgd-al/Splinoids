#include "environment.h"

#include "critter.h"
#include "config.h"

namespace simu {

Environment::Environment(const Genome &g) : _genome(g) {
  _physics = new b2World ({0,0});
}

Environment::~Environment (void) {
  delete _physics;
}

} // end of namespace simu
