#include "ssga.h"

namespace simu {

SSGA::SSGA (void) : _enabled(false), _watching(false), _active(false) {}

void SSGA::clear(void) {}

void SSGA::prePhysicsStep(const std::set<Critter*> &pop) {}
void SSGA::postPhysicsStep(const std::set<Critter*> &pop) {}

} // end of namespace simu
