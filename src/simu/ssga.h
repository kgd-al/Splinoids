#ifndef SSGA_H
#define SSGA_H

#include "../genotype/critter.h"
#include "config.h"

namespace simu {

struct Critter;
struct Environment;

class SSGA {
  uint _minAllowedPopSize;
  bool _enabled, _watching, _active;

  using CGenome = genotype::Critter;
  struct ArchiveItem {
    float fitness;
    const CGenome genome;
    ArchiveItem (const CGenome &g, float f) : fitness(f), genome(g) {}
    friend bool operator< (const ArchiveItem &lhs, const ArchiveItem &rhs) {
      if (lhs.fitness != rhs.fitness) return lhs.fitness < rhs.fitness;
      return lhs.genome.id() < rhs.genome.id();
    }
  };
  using Archive = std::set<ArchiveItem>;
  Archive _archive;
  uint _maxArchiveSize;
  uint _maxGen = 0;

  struct CritterData {
    struct Fields {
      float energy, gametes;
      Fields (void) : energy(0), gametes(0) {}
    };
    P2D pos;
    float distance;

    Fields beforeStep;
    Fields totals;

    uint children;

    CritterData (void) : beforeStep(), totals(), children(0) {}
  };
  using CData = std::map<const Critter*, CritterData>;
  CData _watchData;
  std::map<std::string, float> _champStats;

  using Critters = std::set<Critter*>;

public:
  SSGA();

  void init (uint initPopSize);

  void setEnabled (bool e);
  bool enabled (void) const {   return _enabled;  }

  void update (uint popSize);
  bool watching (void) const {  return _watching; }
  bool active (void) const {    return _active;   }

  bool active (uint popsize) const {
    return popsize < _minAllowedPopSize;
  }

  void clear (void);

  CData::iterator registerBirth (const Critter *newborn);
  void preStep(const Critters &pop);
  void postStep(const Critters &pop);
  void recordChildFor(const Critters &critters);
  void registerDeath (const Critter *deceased);

  float worstFitness (void) const {
    return _archive.empty() ? NAN : _archive.begin()->fitness;
  }

  float averageFitness (void) const {
    if (_archive.empty()) return NAN;
    float f = 0;
    for (const auto &i: _archive) f += i.fitness;
    return f / _archive.size();
  }

  float bestFitness (void) const {
    return _archive.empty() ? NAN : _archive.rbegin()->fitness;
  }

  const auto& champStats (void) const {
    return _champStats;
  }

  genotype::Critter getRandomGenome (rng::FastDice &dice, uint mutations) const;
  genotype::Critter getGoodGenome (rng::FastDice &dice) const;

private:
  void maybePurgeObsoletes(uint gen);
//  void step (const std::set<Critter*> &pop);
};

} // end of namespace simu

#endif // SSGA_H
