#include "ssga.h"
#include "config.h"

namespace simu {

SSGA::SSGA (void) : _enabled(false), _watching(false), _active(false) {}

void SSGA::init(uint initPopSize) {
  _minAllowedPopSize = initPopSize * config::Simulation::ssgaMinPopSizeRatio();
}

void SSGA::update(uint popSize) {
  bool wasWatching = _watching;
  _watching = (popSize < 2*_minAllowedPopSize);
  if (wasWatching && !_watching) {
    _active = false;
    clear();
    return;
  }

  _active = active(popSize);
}

void SSGA::clear(void) {}

void SSGA::prePhysicsStep(const std::set<Critter*> &pop) {}
void SSGA::postPhysicsStep(const std::set<Critter*> &pop) {}

genotype::Critter SSGA::getRandomGenome (rng::FastDice &dice, uint m) const {
  auto g = genotype::Critter::random(dice);
  for (uint i=0; i<m; i++)  g.mutate(dice);
  return g;
}

} // end of namespace simu
