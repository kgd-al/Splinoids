#ifndef SIMULATION_H
#define SIMULATION_H

#include "critter.h"
#include "environment.h"

#include "config.h"

namespace simu {

enum class InitType {
  MUTATIONAL_LANDSCAPE, RANDOM, MEGA_RANDOM, REGULAR
};

class Simulation {
  std::set<Critter*> _critters, _corpses;
  phylogeny::GIDManager _gidManager;

  Environment *_environment;

public:
  using CGenome = Critter::Genome;

  Simulation();
  virtual ~Simulation (void) = default;

  const auto& environment (void) const {
    return *_environment;
  }

  void init (const Environment::Genome &egenome,
             Critter::Genome cgenome, InitType type = InitType::RANDOM);

  virtual void postInit (void) {}

  virtual Critter* addCritter (const CGenome &genome,
                               float x, float y, float r);
  virtual void delCritter (Critter *critter);

  void clear (void);
  virtual void preClear (void) {}

  void step (void);
};

} // end of namespace simu

#endif // SIMULATION_H
