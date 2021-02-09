#include "ssga.h"
#include "config.h"
#include "critter.h"

namespace simu {

static constexpr int debugManagement = 0;
static constexpr int debugSSGA = 0;

SSGA::SSGA (void) : _enabled(false), _watching(false), _active(false) {}

void SSGA::init(uint initPopSize) {
  _minAllowedPopSize = initPopSize * config::Simulation::ssgaMinPopSizeRatio();
  _maxArchiveSize = initPopSize * config::Simulation::ssgaArchiveSizeRatio();
  _enabled = true;

  if (debugSSGA && _enabled)
    std::cerr << "[SSGA] initialized with min pop of " << _minAllowedPopSize
              << " and max archive size of " << _maxArchiveSize << std::endl;

  if (_maxArchiveSize == 0) setEnabled(false);

  update(initPopSize);

  _champStats["Distance"] = NAN;
  _champStats["Energy"] = NAN;
  _champStats["Gametes"] = NAN;
  _champStats["Children"] = NAN;
}

void SSGA::setEnabled (bool e) {
  _enabled = e;
  _watching = _active = false;
}

void SSGA::update(uint popSize) {
  if (!_enabled) return;

  bool wasWatching = _watching;
  _watching = (popSize < 2*_minAllowedPopSize);
  if (wasWatching && !_watching) {
    _active = false;
    clear();
    return;
  }

  _active = active(popSize);

  if (debugSSGA > 1 && _watching) {
    std::cerr << "[SSGA] watching " << _watchData.size() << ", "
              << " archived " << _archive.size() << std::endl;
  }
}

void SSGA::clear(void) {
  _archive.clear();
  _watchData.clear();
}

SSGA::CData::iterator SSGA::registerBirth(const Critter *c) {
  auto p = _watchData.emplace(c, CritterData());
  if (debugManagement)
    std::cerr << "[SSGA] registered birth of " << CID(c) << std::endl;
  return p.first;
}

void SSGA::registerDeath(const Critter *c) {
  auto it = _watchData.find(c);
  if (it == _watchData.end()) return;
  if (debugManagement)
    std::cerr << "[SSGA] registered deletion of " << CID(c) << std::endl;

  CritterData cd = it->second;
  _watchData.erase(it);

//  float fitness =
//        (1+cd.distance)
//      * (1+cd.totals.energy) * (1+cd.totals.gametes)
//      * (1+cd.children);

  float fitness = std::min(cd.distance, 10.f)
                * 10 * std::min(cd.totals.energy, 1.f);
  if (fitness > 0) {
    if (cd.children > 0) fitness *= 2;
    else fitness *= (1+std::min(cd.totals.gametes, 1.f));
  }

  assert(fitness >= 0);

  bool full = (_maxArchiveSize <= _archive.size());
  auto worstItem = _archive.begin();
  float lowestFitness = 0;
  if (!_archive.empty())  lowestFitness = worstItem->fitness;

  if (debugSSGA > 1) {
    std::cerr << "Registered death of " << CID(c) << " with fitness "
              << "(1+" << cd.totals.energy << ")*(1+" << cd.totals.gametes
              << ")*(1+" << cd.children << ") = " << fitness << " ";

    std::cerr << (fitness < lowestFitness ? "<" : ">=") << " " << lowestFitness
              << " -> ";
    if (!full)
      std::cerr << "inserted";
    else if (fitness >= lowestFitness)
      std::cerr << "replaces";
    else
      std::cerr << "forgotten";
    std::cerr << std::endl;
  }

  bool champ = (fitness >= bestFitness());
  bool insert = (!full || fitness >= lowestFitness);
  if (insert) _archive.emplace(c->genotype(), fitness);

  if (debugManagement && full && fitness >= lowestFitness)
    std::cerr << "\t\tErasing 0x" << std::hex << worstItem->genome.id()
              << std::dec << " (" << worstItem->fitness << ")" << std::endl;
  if (full && fitness >= lowestFitness) _archive.erase(worstItem);

  if (insert) maybePurgeObsoletes(c->genotype().gdata.generation);

  if (champ) {
    _champStats["Distance"] = cd.distance;
    _champStats["Energy"] = cd.totals.energy;
    _champStats["Gametes"] = cd.totals.gametes;
    _champStats["Children"] = cd.children;
//    std::cerr << "New champ:\n";
//    for (const auto &p: _champStats)
//      std::cerr << "\t" << p.first << ": " << p.second << "\n";
//    std::cerr << "\n";
  }
}

void SSGA::preStep(const std::set<Critter*> &pop) {
  for (Critter *c: pop) {
    auto it = _watchData.find(c);
    if (it == _watchData.end()) it = registerBirth(c);
    CritterData &cd = it->second;
    cd.pos = c->pos();
    cd.beforeStep.energy = c->usableEnergy();
    cd.beforeStep.gametes = c->reproductionReserve();

    if (debugManagement > 1)
      std::cerr << "[SSGA] updated energy cache data for  "
                << CID(c) << std::endl;
  }
}

void SSGA::postStep(const std::set<Critter*> &pop) {
  for (Critter *c: pop) {
    auto it = _watchData.find(c);
    if (it == _watchData.end()) it = registerBirth(c);
    CritterData &cd = it->second;

    cd.distance += (c->pos() - cd.pos).Length();

    if (c->usableEnergy() / c->maxUsableEnergy() < .75)
      cd.totals.energy +=
          std::max(c->usableEnergy() - cd.beforeStep.energy, 0.);

    cd.totals.gametes += std::max(c->reproductionReserve() - cd.beforeStep.gametes, 0.);

    if (debugManagement > 1)
      std::cerr << "[SSGA] updated energy data for  " << CID(c) << std::endl;
  }
}

void SSGA::recordChildFor(const std::set<Critter *> &critters) {
  for (Critter *c: critters) {
    auto it = _watchData.find(c);
    if (it == _watchData.end()) it = registerBirth(c);
    CritterData &cd = it->second;
    cd.children++;

    if (debugManagement > 1)
      std::cerr << "[SSGA] updated child data for  " << CID(c) << std::endl;
  }
}

void SSGA::maybePurgeObsoletes(uint gen) {
  static const auto gspan = config::Simulation::ssgaMaxGenerationalSpan();

  if (gen < _maxGen)  return;
  _maxGen = gen;

  if (_maxGen <= gspan) return;

  uint minGen = gen - gspan;
  if (debugSSGA)
    std::cerr << "[SSGA] Purging archive of all genomes older than gen "
              << minGen << std::endl;

  auto it = _archive.begin();
  while (it != _archive.end()) {
    if (it->genome.gdata.generation < minGen)
      it = _archive.erase(it);
    else
      ++it;
  }
}

genotype::Critter SSGA::getRandomGenome (rng::FastDice &dice, uint m) const {
  auto g = genotype::Critter::random(dice);
  for (uint i=0; i<m; i++)  g.mutate(dice);
  return g;
}

genotype::Critter SSGA::getGoodGenome(rng::FastDice &dice) const {
  std::vector<float> values (_archive.size());
  uint i=0;
  for (const auto &item: _archive) values[i++] = item.fitness;
  rng::rdist dist (values.begin(), values.end());

  i = dice(dist);
  auto it = _archive.begin();
  std::advance(it, i);
  CGenome g = it->genome;
  while (dice(config::Simulation::ssgaMutationProbability())) g.mutate(dice);
  g.cdata.sex = dice(.5) ? Critter::Sex::FEMALE : Critter::Sex::MALE;

  if (debugSSGA)
    std::cerr << "Returning genome of generation " << g.gdata.generation
              << " and fitness " << it->fitness << std::endl;

  return g;
}

} // end of namespace simu
