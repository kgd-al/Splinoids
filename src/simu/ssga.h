#ifndef SSGA_H
#define SSGA_H

#include "../genotype/critter.h"

namespace simu {

struct Critter;
struct Environment;

class SSGA {
  bool _enabled, _watching, _active;

public:
  SSGA();

  void setEnabled (bool e) {    _enabled = e;     }
  bool enabled (void) const {   return _enabled;  }

  void setWatching (bool w) {   _watching = w;    }
  bool watching (void) const {  return _watching; }

  void setActive (bool a) {     _active = a;      }
  bool active (void) const {    return _active;   }

  void clear (void);

  void prePhysicsStep(const std::set<Critter*> &pop);
  void postPhysicsStep(const std::set<Critter*> &pop);

  genotype::Critter getRandomGenome (rng::FastDice &dice) const {
    return genotype::Critter::random(dice);
  }

  genotype::Critter getGoodGenome (rng::FastDice &dice) const;

//  void step (const std::set<Critter*> &pop);
};

} // end of namespace simu

#endif // SSGA_H
