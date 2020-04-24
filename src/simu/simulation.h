#ifndef SIMULATION_H
#define SIMULATION_H

#include <chrono>

#include "critter.h"
#include "foodlet.h"
#include "environment.h"
#include "ssga.h"

#include "time.h"
#include "config.h"

DEFINE_PRETTY_ENUMERATION(SimuFields, ENV, CRITTERS, FOODLETS, PTREE)

namespace simu {

class Simulation {
protected:
  phylogeny::GIDManager _gidManager;
  std::set<Critter*> _critters;

  uint _nextFoodletID;
  std::set<Foodlet*> _foodlets;

  std::unique_ptr<Environment> _environment;

  Time _startTime, _time, _endTime;
  uint _minGen, _maxGen;

  SSGA _ssga;

  std::ofstream _statsLogger;

  decimal _systemExpectedEnergy;

  uint _stepTimeMs,
        _splnTimeMs, _envTimeMs, _decayTimeMs, _regenTimeMs;

  struct ReproductionStats {
    uint attempts = 0, sexual = 0, asexual = 0, ssga = 0;
  } _reproductions;

  struct Autopsies {
    enum Age { YOUNG, ADULT, OLD };
    enum Death { INJURY, STARVATION };
    std::array<std::array<uint, 3>, 2> counts {{0}};
    uint oldage = 0;
    uint total (void) const {
      uint t = 0;
      for (const auto &a: counts) for (const auto &c: a)  t += c;
      return t + oldage;
    }
  } _autopsies;

  bool _aborted;

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
    return _aborted
        || _critters.empty()
        || (_startTime < _endTime && _endTime <= _time);
  }

  bool aborted (void) const {
    return _aborted;
  }

  virtual void abort (void) {
    _aborted = true;
  }

  auto& environment (void) {
    return *_environment;
  }

  const auto& environment (void) const {
    return *_environment;
  }

  auto& physics (void) {
    return _environment->physics();
  }

  const auto& currTime (void) const {
    return _time;
  }

  auto& dice (void) {
    return environment().dice();
  }

  auto minGeneration (void) const {
    return _minGen;
  }

  auto maxGeneration (void) const {
    return _maxGen;
  }

  enum Overwrite : char {
    UNSPECIFIED = '\0',
    ABORT = 'a',
    PURGE = 'p'
  };
  bool setDataFolder (const stdfs::path &path, Overwrite o = UNSPECIFIED);

  void init (const Environment::Genome &egenome,
             Critter::Genome cgenome, const InitData &data = InitData{});

  virtual void postInit (void) {}

  virtual Critter* addCritter (const CGenome &genome,
                               float x, float y, float a, decimal e,
                               float age = 0);
  virtual void delCritter (Critter *critter);

  virtual Foodlet* addFoodlet (BodyType t, float x, float y, float r, decimal e);
  virtual void delFoodlet (Foodlet *foodlet);

  void clear (void);
  virtual void preClear (void) {}

  virtual void step (void);

  void atEnd (void);

  const auto& critters (void) const {
    return _critters;
  }

  decimal totalEnergy(void) const;

  stdfs::path periodicSaveName (void) const {
    return periodicSaveName("./", _time, _minGen, _maxGen);
  }

  static stdfs::path periodicSaveName(const stdfs::path &folder,
                                      const Time &time,
                                      uint minGen, uint maxGen) {
    std::ostringstream oss;
    oss << time.pretty() << "_g" << minGen << "-" << maxGen;
    oss << ".save";
    return folder / oss.str();
  }

  void serializePopulations (nlohmann::json &jcritters,
                             nlohmann::json &jfoodlets) const;

  void deserializePopulations (const nlohmann::json &jcritters,
                               const nlohmann::json &jfoodlets,
                               bool updateTree);

  void save (stdfs::path file) const;
  static void load (const stdfs::path &file, Simulation &s,
                    const std::string &constraints, const std::string &fields);

  friend void assertEqual (const Simulation &lhs, const Simulation &rhs,
                           bool deepcopy);

  static void printStaticStats (void);

protected:
  struct Stats {
    uint ncritters = 0, ncorpses = 0, nplants = 0;
    uint nyoungs = 0, nadults = 0, nelders = 0;
    uint nfights = 0, nfeedings = 0;

    decimal ecritters = 0, ecorpses = 0, eplants = 0, ereserve = 0;

    float fmin = 0, favg = 0, fmax = 0;
  };
  virtual void processStats (const Stats&) const {}

  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;

  static time_point now (void) {  return clock::now(); }

  template <typename D = std::chrono::milliseconds>
  static auto durationFrom (const time_point &start) {
    return std::chrono::duration_cast<D>(now() - start).count();
  }

private:
  b2Body* critterBody (float x, float y, float a);
  b2Body* foodletBody (float x, float y);

  void reproduction (void);
  Critter* createChild (const Critter *parent, const CGenome &genome,
                        decimal energy, rng::AbstractDice &dice);

  void produceCorpses (void);
  void steadyStateGA (void);

  void decomposition (void);
  void plantRenewal (float bounds = -1);

  void logStats (void);

  // Compensate for variations in total energy
  void correctFloatingErrors (void);

#ifndef NDEBUG
  void detectBudgetFluctuations (float threshold = config::Simulation::epsilonE);
#else
#endif
};

} // end of namespace simu

#endif // SIMULATION_H
