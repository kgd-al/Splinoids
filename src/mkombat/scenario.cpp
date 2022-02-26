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

void Team::toFile(const stdfs::path &p) {
  std::ofstream ofs (p);
  ofs << nlohmann::json(*this);
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

  if (tSize == 1) {
    e.width = e.height = 5;

  } else
    utils::Thrower<std::logic_error>("Non atomic teams not implemented!");

  return e;
}

// =============================================================================

Scenario::Scenario (Simulation &simulation, uint tSize)
  : _simulation(simulation), _teamsSize(tSize) {
  simulation.setupCallbacks({
    { Simulation::POST_ENV_STEP,  [this] { postEnvStep(); } },
    {     Simulation::POST_STEP,  [this] { postStep();    } },
    { Simulation::PRE_CORPSE_DEL, [this] (Critter *c) {
        preDelCritter(c);
      }
    }
  });
}

void Scenario::init(const Params &params) {
  static constexpr auto E = INFINITY;
  static constexpr auto R = Critter::MAX_SIZE;

  using F = simu::Scenario::Params::Flag;

  _flags = params.flags;
  _currentFlags.reset();
  if (_flags.any()) std::cerr << "Using flags: " << _flags << "\n";

  _simulation.init(environmentGenome(_teamsSize), {}, commonInitData);

  assert(params.lhs.size() == 1);  // cannot manage teams for now

  if (params.lhs.size() != _teamsSize)
    utils::Thrower("Provided lhs team has size ", params.lhs.size(),
                   " instead of", _teamsSize);

  if (params.rhs.size() != _teamsSize)
    utils::Thrower("Provided rhs team has size ", params.rhs.size(),
                   " instead of", _teamsSize);

  float W = _simulation.environment().xextent(),
        H = _simulation.environment().yextent();

//  lhs.gdata.self.gid = phylogeny::GID(0);

  static const auto x = [] (int team) { return 1*R*(2*team-1); };
  static const auto y = [] (int /*i*/) { return 0; };
  static const auto pin = [] (auto c) {
    c->immobile = true;
    c->body().SetType(b2_staticBody);
    for (auto a: c->arms()) if (a) a->SetType(b2_staticBody);
  };
  if (neuralEvaluation()) {
    auto subject =
      _simulation.addCritter(params.lhs.members[0],
                             x(0), y(0), 0, E, .5, true);
    _teams[0].insert(subject);
    pin(subject);

    if (_flags.test(F::PAIN_OTHER)) {
      auto opponent =
        _simulation.addCritter(params.rhs.members[0],
                               x(1), y(0), M_PI, E, .5, true);
      _teams[1].insert(opponent);
      pin(opponent);

    } else if (_flags.test(F::PAIN_ALLY)) {
      if (_teamsSize != 2)
        utils::Thrower("Cannot test empathy on anything but pairs (provided "
                       "team size is", _teamsSize, ")");
      utils::Thrower("Empathy test not implemented");
    }


  } else
    for (int t = 0; t < 2; t++) {
      const Team &team = (t==0 ? params.lhs : params.rhs);
      for (uint i=0; i<_teamsSize; i++)
        _teams[t].insert(
          _simulation.addCritter(team.members[i],
                                 x(t), y(i), t*M_PI, E, .5, true));
    }

  assert(_simulation.critters().size()
         == (1+(!params.flags.test(F::PAIN_SELF)))*_teamsSize);
  std::set<phylogeny::GID> gids;
  for (auto &t: _teams) {
    for (auto c: t) {
      auto r = gids.insert(c->id());
      assert(r.second);
      (void)r;
    }
  }

  for (auto &a: _autopsies) a.fill(0);

//  std::cerr << "Pinning subjects in place\n";
  if (false) {
    std::cerr << "Pinning subjects in place (by deactivating neural outputs)\n";
    for (auto &team: _teams)
      for (Critter *c: team)
        c->immobile = true;

  } else if (false) {
    std::cerr << "Pinning rhs team in place (by deactivating neural outputs)\n";
    for (Critter *c: _teams[1])
        c->immobile = true;

  } else if (false) {  // impolite pinning
    std::cerr << "Pinning subjects in place (by settings mass to 0)\n";
    for (auto &team: _teams)
      for (Critter *c: team)
        c->body().SetType(b2_staticBody);
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

void Scenario::postEnvStep(void) {}

void Scenario::postStep(void) {
  static constexpr auto INJURY = .9;
  static constexpr auto NEUTRAL_FIRST = false;

  static const auto timeout = DURATION*config::Simulation::ticksPerSecond();
  static const auto PERIOD = 4 * config::Simulation::ticksPerSecond();
  const auto t = _simulation.currTime().timestamp();

  if (_flags.any()) {
    const auto step = t / PERIOD;
    if (step >= 6)  _simulation._finished = true;

    if (!((t-1) % PERIOD == 0)) return;

//    std::cerr << "## Period " << step << "\n";

    Critter *sbj = subject();

    if (_flags[Params::PAIN_SELF]) {  // Pain
      sbj->overrideBodyHealthness(1 - (step % 2 == NEUTRAL_FIRST) * INJURY);
      _currentFlags.flip(Params::PAIN_SELF);
    }

    if (_flags[Params::PAIN_OTHER]) {  // Injured opponent
      Critter *adv = opponent();
      adv->overrideBodyHealthness(1 - (step % 2 == NEUTRAL_FIRST) * INJURY);
      _currentFlags.flip(Params::PAIN_OTHER);
    }

    return;
  }

  bool empty = _teams[0].empty() || _teams[1].empty();
  bool awake = false;
  for (const auto &team: _teams) {
    for (const Critter *c: team) {
      awake |= c->body().IsAwake();
      if (awake)  break;
    }
    if (awake)  break;
  }

  _simulation._finished = empty || !awake || (t >= timeout);
}

void Scenario::preDelCritter(Critter *c) {
//  std::cerr << CID(c) << " is dead\n";
  static const auto autopsy = [] (const Critter *c) {
    if (c->starved())
      return DeathCause::STARVATION;
    else if (c->fatallyInjured())
      return DeathCause::INJURY;
    else
      return UNKNOWN_DEATH_CAUSE;
  };

  for (uint i=0; i<_teams.size(); i++) {
    auto &team = _teams[i];
    auto it = team.find(c);
    if (it != team.end()) {
      _autopsies[i][autopsy(c)]++;
      team.erase(it);
    }
  }
}

float Scenario::score (void) const {
  std::array<float,2> healths;
  float score;
  for (uint t: {0, 1}) {
    healths[t] = 0;
    for (simu::Critter *c: _teams[t])
      healths[t] += c->bodyHealthness();
    if (!_teams[t].empty())  healths[t] /= _teamsSize;
  }

  const auto A = healths[0], B = healths[1];
  if (A == 0 && B == 0)
    score = 0;

  else if (A > 0 && B == 0)
    score = 2;

  else if (A == 0 && B > 0)
    score = -2;

  else if (A < 1 || B < 1)
    score = -B+A;

  else {
    float d = std::numeric_limits<float>::max();
    for (simu::Critter *lhs: _teams[0])
      for (simu::Critter *rhs: _teams[1])
        d = std::min(d, b2Distance(lhs->pos(), rhs->pos()));

    const auto &e = _simulation.environment();
    d /= std::sqrt(std::pow(e.width(), 2) + std::pow(e.height(), 2));
    d = - 3 - d;

    score = d;
  }

  return score;
}

std::array<bool,2> Scenario::brainless(void) const {
  std::array<bool,2> b {false,false};

  for (uint t: {0,1})
    for (auto it = _teams[t].begin(); it != _teams[t].end() && !b[t]; ++it)
      b[t] |= (*it)->brain().empty();

  return b;
}

} // end of namespace simu
