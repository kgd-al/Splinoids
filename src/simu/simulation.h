#ifndef SIMULATION_H
#define SIMULATION_H

#include "critter.h"
#include "foodlet.h"
#include "environment.h"
#include "ssga.h"

#include "time.h"
#include "config.h"

namespace simu {

class Simulation {
protected:
  phylogeny::GIDManager _gidManager;
  std::set<Critter*> _critters;

  uint _nextPlantID;
  std::set<Foodlet*> _foodlets;

  std::unique_ptr<Environment> _environment;
  rng::FastDice _dice;

  Time _startTime, _time, _endTime;
  uint _minGen, _maxGen;

  SSGA _ssga;

  std::ofstream _statsLogger;

  float _systemExpectedEnergy;

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

  bool finished (void) const {
    return _endTime <= _time;
  }

  auto& environment (void) {
    return *_environment;
  }

  auto& physics (void) {
    return _environment->physics();
  }

  const auto& currTime (void) const {
    return _time;
  }

  void init (const Environment::Genome &egenome,
             Critter::Genome cgenome, const InitData &data = InitData{});

  virtual void postInit (void) {}

  virtual Critter* addCritter (const CGenome &genome,
                               float x, float y, float a, float e);
  virtual void delCritter (Critter *critter);

  virtual Foodlet* addFoodlet (BodyType t, float x, float y, float r, float e);
  virtual void delFoodlet (Foodlet *foodlet);

  void clear (void);
  virtual void preClear (void) {}

  virtual void step (void);

  float totalEnergy (void) const;

protected:
  struct Stats {
    uint ncritters, ncorpses, nplants;
    uint nfights, nfeedings;

    float ecritters, ecorpses, eplants, ereserve;
  };
  virtual void processStats (const Stats&) const {}

private:
  void produceCorpses (void);
  void steadyStateGA (void);

  void decomposition (void);
  void plantRenewal (float bounds = -1);

  void logStats (void);

  // Compensate for variations in total energy
  void correctFloatingErrors (void);
};

} // end of namespace simu

#endif // SIMULATION_H
