#ifndef SSGA_H
#define SSGA_H

#include "../genotype/critter.h"

namespace simu {

struct Critter;
struct Environment;

class SSGA {
  uint _minAllowedPopSize;
  bool _enabled, _watching, _active;

public:
  SSGA();

  void init (uint initPopSize);

  void setEnabled (bool e) {    _enabled = e;     }
  bool enabled (void) const {   return _enabled;  }

  void update (uint popSize);
  bool watching (void) const {  return _watching; }
  bool active (void) const {    return _active;   }

  bool active (uint popsize) const {
    return popsize < _minAllowedPopSize;
  }

  void clear (void);

  void prePhysicsStep(const std::set<Critter*> &pop);
  void postPhysicsStep(const std::set<Critter*> &pop);

  genotype::Critter getRandomGenome (rng::FastDice &dice, uint mutations) const;
  genotype::Critter getGoodGenome (rng::FastDice &dice) const;

//  void step (const std::set<Critter*> &pop);
};

} // end of namespace simu

#endif // SSGA_H
