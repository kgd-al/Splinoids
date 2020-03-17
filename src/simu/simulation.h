#ifndef SIMULATION_H
#define SIMULATION_H

#include "critter.h"
#include "plant.h"
#include "environment.h"

#include "time.h"
#include "config.h"

namespace simu {

class Simulation {
protected:
  std::set<Critter*> _critters;
  phylogeny::GIDManager _gidManager;

  std::set<Plant*> _plants;

  std::unique_ptr<Environment> _environment;
  float _energyReserve;
  rng::FastDice _dice;

  Time _time;
  uint _minGen, _maxGen;


public:
  struct InitData {
    float ienergy;  // Initial energy amount (should remain constant)
    float cRatio;   // Portion of initial energe alloted to critters

    uint nCritters; // Initial number of critters

    float cRange;   // Critters initial coordinates are in [-range,range]^2
    float pRange;   // Plants initial coordinates are in [-range,range]^2

    int seed;

    InitData (void) : ienergy(1000), cRatio(.5),
                      nCritters(ienergy*cRatio/2),
                      cRange (10), pRange(20), seed(-1) {}
  };

  using CGenome = Critter::Genome;

  Simulation();
  virtual ~Simulation (void);

  auto& environment (void) {
    return *_environment;
  }

  auto& physics (void) {
    return _environment->physics();
  }

  void init (const Environment::Genome &egenome,
             Critter::Genome cgenome, const InitData &data = InitData{});

  virtual void postInit (void) {}

  virtual Critter* addCritter (const CGenome &genome,
                               float x, float y, float e);
  virtual void delCritter (Critter *critter);

  virtual Plant* addPlant (float x, float y, float s, float e);
  virtual void delPlant (Plant *plant);

  void clear (void);
  virtual void preClear (void) {}

  virtual void step (void);

  float totalEnergy (void) const;
};

} // end of namespace simu

#endif // SIMULATION_H
