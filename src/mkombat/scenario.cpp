#include "scenario.h"

namespace simu {

static constexpr auto R = Critter::MAX_SIZE * Critter::RADIUS;
static constexpr float fPI = M_PI;
static constexpr float TARGET_ENERGY = .90;

static constexpr bool debugAI = false;

static const P2D nanPos {NAN,NAN};


// =============================================================================

void to_json (nlohmann::json &j, const Team &t) {
  j = t.members;
}

void from_json (const nlohmann::json &j, Team &t) {
  t.members = j.get<decltype(t.members)>();
}

Team Team::fromFile (const stdfs::path &p) {
  return nlohmann::json::parse(utils::readAll(p));
}

// =============================================================================

const Simulation::InitData Scenario::commonInitData = [] {
  Simulation::InitData d;
  d.ienergy = 0;
  d.cRatio = 0;
  d.nCritters = 0;
  d.cRange = 0;
  d.pRange = 0;
  d.seed = 0;
  d.cAge = .5;
  return d;
}();

genotype::Environment Scenario::environmentGenome (uint tSize) {
  genotype::Environment e;
  e.maxVegetalPortion = 1;
  e.taurus = 0;

  if (tSize == 1)
    e.width = e.height = 4;

  else
    utils::doThrow<std::logic_error>("Non atomic teams not implemented!");

  return e;
}

// =============================================================================

Scenario::Scenario (Simulation &simulation, uint tSize)
  : _simulation(simulation), _teamsSize(tSize) {
  simulation.setupCallbacks({
    { Simulation::POST_ENV_STEP, [this] { postEnvStep(); } },
    {     Simulation::POST_STEP, [this] { postStep();    } },
  });
}

void Scenario::init(const Team &lhs, const Team &rhs) {
  static constexpr auto E = Critter::maximalEnergyStorage(Critter::MAX_SIZE);
  _simulation.init(environmentGenome(_teamsSize), {}, commonInitData);

  if (lhs.size() != _teamsSize)
    utils::doThrow<std::invalid_argument>(
      "Provided lhs team has size ", lhs.size(), " instead of",
      _teamsSize);

  if (rhs.size() != _teamsSize)
    utils::doThrow<std::invalid_argument>(
          "Provided rhs team has size ", rhs.size(), " instead of",
          _teamsSize);

  float W = _simulation.environment().xextent(),
        H = _simulation.environment().yextent();

//  lhs.gdata.self.gid = phylogeny::GID(0);

  for (uint t: {0,1}) {
    const Team &team = (t ? lhs : rhs);
    for (uint i=0; i<_teamsSize; i++)
      _teams[t].push_back(
        _simulation.addCritter(team.members[i], 2*(2*t-1), 0, 0, E, .5));
  }
}

//void Scenario::applyLesions(int lesions) {
//  if (lesions == 0) return;
//  std::cout << "Applying lesion type: " << lesions;

//  uint count = 0;
//  for (auto &ptr: _subject->brain().neurons()) {
//    phenotype::ANN::Neuron &n = *ptr;
//    for (auto it=n.links().begin(); it!=n.links().end(); ) {
//      phenotype::ANN::Neuron &tgt = *(it->in.lock());
//      if (tgt.type == phenotype::ANN::Neuron::I
//          && (lesions == 3
//            || (lesions == 1 && tgt.pos.y() < -.75)   // deactivate vision
//            || (lesions == 2 && tgt.pos.y() >= -.75)  // deactivate audition
//            || (n.flags == 1024 && ( // deactivate amygdala inputs
//                 (lesions == 4 && tgt.pos.y() == -1)    // deactivate red
//              || (lesions == 5 && tgt.pos.y() == -.75))))) { // deactivate noise audition
//        it = n.links().erase(it);
//        count++;
////        std::cerr << "Deactivated " << tgt.pos << " -> " << n.pos << "\n";
//      } else {
//        ++it;
////        std::cerr << "Kept " << tgt.pos << " -> " << n.pos << " (" << n.flags << ")\n";
//      }
//    }
//  }

//  std::cout << " (" << count << " links deleted)\n";
//}

void Scenario::postEnvStep(void) {
}

void Scenario::postStep(void) {
  _simulation._finished = _simulation.currTime().timestamp() >= 1000;
}

std::array<float,2> Scenario::scores (void) const {
  /// TODO Does not work for dead critters...
  std::array<float,2> healths, scores;
  for (uint t: {0, 1}) {
    healths[t] = 0;
    for (simu::Critter *c: _teams[t])
      healths[t] += c->bodyHealthness();
    healths[t] /= _teamsSize;
  }

  const auto A = healths[0], B = healths[1];
  if (A == 0 && B == 0)
    scores = { 0, 0 };

  else if (A > 0 && B == 0)
    scores = { 2, -2 };

  else if (A == 0 && B > 0)
    scores = { -2, 2 };

  else if (A < 1 || B < 1)
    scores = { -B+A, -A+B };  // 1-B - (1-A)

  else {
    float d = std::numeric_limits<float>::max();
    for (simu::Critter *lhs: _teams[0])
      for (simu::Critter *rhs: _teams[1])
        d = std::min(d, b2Distance(lhs->pos(), rhs->pos()));
    d = - 3 - d;
    scores = { d, d };
  }

  return scores;
}

std::array<bool,2> Scenario::brainless(void) const {
  std::array<bool,2> b {true,true};

  for (uint t: {0,1})
    for (uint i=0; i<_teamsSize && b[t]; i++)
      b[t] |= _teams[t][i]->brain().empty();

  return b;
}

} // end of namespace simu
