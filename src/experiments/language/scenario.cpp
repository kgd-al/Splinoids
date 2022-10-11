#include "scenario.h"

namespace simu {

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

genotype::Environment Scenario::environmentGenome (bool eval) {
  genotype::Environment e;
  e.maxVegetalPortion = 1;
  e.taurus = 0;

  if (eval) e.width = e.height = 50;
  else      e.width = 2*(e.height = 5);

  return e;
}

// =============================================================================

Scenario::Scenario (Simulation &simulation) : _simulation(simulation) {
  simulation.setupCallbacks({
    { Simulation::POST_ENV_STEP,  [this] { postEnvStep(); } },
    {     Simulation::POST_STEP,  [this] { postStep();    } },
    { Simulation::PRE_CORPSE_DEL, [this] (Critter *c) {
        preDelCritter(c);
      }
    }
  });
}

Critter* Scenario::makeCritter (uint id, const genotype::Critter &genome,
                                const phenotype::ANN *brainTemplate) {

  static constexpr auto E = INFINITY;

  const auto x = [this] (int id) {
    return (1-2*id)
        /** _simulation.environment().xextent() / 3.f*/;
  };
  const auto pin = [] (auto c) {
    c->immobile = true;
//    c->paralyzed = true;
//    c->body().SetType(b2_staticBody);
  };

  auto c = _simulation.addCritter(genome, x(id), 0,
                                  id*M_PI, E, .5, true,
                                  brainTemplate);
  _critters.push_back(c);
  if (id == 0 || neuralEvaluation()) pin(c);  // emitter cannot move

//  if (team == 0 && id == 0)  c->mute = true;

  return c;
}

Foodlet* Scenario::makeFoodlet (float x, int side, Color c) {
  const float W = _simulation.environment().xextent(),
              H = _simulation.environment().yextent();

  float fr = .25, fe = Foodlet::maxStorage(BodyType::PLANT, fr);
  auto fy = -2 * side * (x-.5f) * std::min(H-fr, 1 * H);

  auto f = _simulation.addFoodlet(BodyType::PLANT,
                                  side * (W-fr), fy, fr, fe);
  f->setBaseColor(c);
  _foodlets.push_back(f);

  return f;
}

void Scenario::init(const Params &params) {
  _params = params;

  // Will be flipped at the end of the first step
//  if (!params.neutralFirst) _currentFlags.reset();
//  else                      _currentFlags = params.flags;

  if (neuralEvaluation())
    std::cout << "Using flags: " << params.flags << "\n";

  _simulation.init(environmentGenome(neuralEvaluation()),
                   {}, commonInitData);

  // Deactivate energy monitoring
  if (neuralEvaluation()) _simulation._systemExpectedEnergy = -1;

  if (neuralEvaluation()) {
//    config::Simulation::combatBaselineIntensity.overrideWith(0);
//    makeCritter(0, 0, params.lhs.genome);
//    if (!params.neutralFirst) {
//      if (hasFlag(Params::WITH_ALLY)) makeCritter(1, 0, _params.lhs.genome);
//      if (hasFlag(Params::WITH_OPP1) || hasFlag(Params::WITH_OPP2))
//        makeCritter(1, 0, _params.rhs.genome);
//    }

//    if (hasFlag(Params::SOUND_FRND) || hasFlag(Params::SOUND_OPPN)) {
//      std::istringstream iss (params.arg);
//      iss >> _testChannel;
//      if (!iss)
//        utils::Thrower("Failed to parse ", params.arg, " as a channel number");
//      if (Critter::VOCAL_CHANNELS < _testChannel)
//        utils::Thrower("Channel id is out of range [0, ",
//                       Critter::VOCAL_CHANNELS, "]");
//    }

  } else {
    for (uint i=0; i<2; i++)
      makeCritter(i, params.genome, params.brainTemplate);

    const float H = _simulation.environment().yextent();
    float width = .25;
    _simulation.addObstacle(-.5*width, -H, width, 2*H);

    if (params.type == Type::DIRECTION) {
      for (uint i=0; i<3; i++) makeFoodlet(i/2.f, -1);
      makeFoodlet(params.spec.target/2.f, 1);
      assert(_foodlets.size() == 4);

    } else if (params.type == Type::COLOR) {
      float n = params.spec.colors.size() - 1;
      for (uint i=0; i<n; i++)
        makeFoodlet(1-i/(n-1), -1, params.spec.colors[i]);
      makeFoodlet((n-1)/2.f, 1, params.spec.colors.back());
    }
  }

  _mute = true;

  std::set<phylogeny::GID> gids;
  for (auto &c: _critters) {
    auto r = gids.insert(c->id());
    assert(r.second);
    (void)r;
  }
}

char Scenario::critterRole(uint id) {
  switch (id) {
  case 0: return 'e';
  case 1: return 'r';
  default:
    utils::Thrower("No role for critter id ", id);
    return '!';
  }
}

void Scenario::postEnvStep(void) {}

void Scenario::postStep(void) {
//  static constexpr auto INJURY = .9;

  static const auto TIMEOUT = DURATION*config::Simulation::ticksPerSecond();
//  static const auto PERIOD = 4 * config::Simulation::ticksPerSecond();
  const auto t = _simulation.currTime().timestamp();

//  static const auto injectSound = [] (simu::Critter *c, uint channel) {
//    auto &ears = c->ears();
//    ears.fill(0);
//    ears[channel] = ears[channel+Critter::VOCAL_CHANNELS+1] = 1;
//  };

  _mute &= emitter()->silent(false);

  if (neuralEvaluation()) {
//    const auto step = t / PERIOD;
//    if (step >= 6)  _simulation._finished = true;

//    Critter *sbj = subject();

//    sbj->body().SetTransform(sbj->body().GetPosition(),
//                             std::sin(2*M_PI*t / PERIOD) * M_PI / 4);

//    bool neutral = (step%2 != _params.neutralFirst);

//    if (!neutral) {
//      if (hasFlag(Params::SOUND_NOIS)) injectSound(sbj, 0);
//      if (hasFlag(Params::SOUND_FRND) || hasFlag(Params::SOUND_OPPN))
//        injectSound(sbj, _testChannel);
//    }

//    if (!((t-1) % PERIOD == 0)) return;

////    std::cerr << "## Period " << step << "\n";
//    // Maybe create stuff
//    if (neutral) {  // Clean previous
//      if (!_teams[1].empty()) {
//        for (auto &c: _teams[1])  _simulation.delCritter(c);
//        _teams[1].clear();
//      }

//    } else if (step > 0) {
//      if (hasFlag(Params::WITH_ALLY)) makeCritter(1, 0, _params.lhs.genome);
//      if (hasFlag(Params::WITH_OPP1) || hasFlag(Params::WITH_OPP2))
//        makeCritter(1, 0, _params.rhs.genome);
//    }

//    // Apply damages
//    if (hasFlag(Params::PAIN_ABSL)) {
//      damage(sbj, !neutral, false);
//      _currentFlags.flip(Params::PAIN_ABSL);
//    }

//    if (hasFlag(Params::PAIN_INST)) {
//      sbj->inPain = neutral ? -1 : INJURY;
//      _currentFlags.flip(Params::PAIN_INST);
//    }

//    if (hasFlag(Params::TOUCH)) {
//      sbj->overrideTouchSensor(0, !neutral);
//      _currentFlags.flip(Params::TOUCH);
//    }

//    // manage remaining flags
//    if (hasFlag(Params::WITH_ALLY)) _currentFlags.flip(Params::WITH_ALLY);
//    if (hasFlag(Params::WITH_OPP1)) _currentFlags.flip(Params::WITH_OPP1);
//    if (hasFlag(Params::WITH_OPP2)) _currentFlags.flip(Params::WITH_OPP2);

//    // Manage sound
//    if (hasFlag(Params::SOUND_NOIS)) _currentFlags.flip(Params::SOUND_NOIS);
//    if (hasFlag(Params::SOUND_FRND)) _currentFlags.flip(Params::SOUND_FRND);
//    if (hasFlag(Params::SOUND_OPPN)) _currentFlags.flip(Params::SOUND_OPPN);

//    return;
  }

//  bool casualty = (_teams[0].size() < _teamsSize)
//               || (_teams[1].size() < _teamsSize);

  _simulation._finished =
       (t >= TIMEOUT)
    || !receiver()->body().IsAwake()
    || (!_simulation.environment().feedingEvents().empty());
}

void Scenario::preDelCritter(Critter *c) {(void)c;}

float Scenario::minScore(void) { return -1; }

// Theoretical
float Scenario::maxScore(void) { return 2; }

float Scenario::score (void) const {
  if (_simulation._aborted) return -std::numeric_limits<float>::max();

  float score = 0;

  if (_mute || emitter()->brain().empty())
    score = minScore();

  else {
    simu::Environment::FeedingEvents events =
      _simulation.environment().feedingEvents();

    int success = 0;
    if (events.empty()) {
      const auto dist = [this] (uint id, const simu::Critter *c) {
        return (_foodlets[id]->body().GetWorldCenter() - c->pos()).Length();
      };

      int target = _params.spec.target;
      if (_params.type == Type::COLOR) { // Must manually look for the target
        target = -1;
        for (uint i=0; i<_foodlets.size()-1; i++)
          if (_foodlets[i]->baseColor() == _params.spec.colors.back())
            target = i;
      }

      if (target >= 0)
        score += 1 - dist(target, receiver())
                     / dist(_foodlets.size()-1, emitter());

    } else {
      assert(events.size() == 1);

      auto e = *events.begin();
      bool match = false;
      if (_params.type == Type::DIRECTION)
        match = (e.foodlet == _foodlets[_params.spec.target]);
      else
        match = (e.foodlet->baseColor() == _params.spec.colors.back());

      success = match ? 1 : -1;
      score += success;
    }

    if (success == 1)
      for (const simu::Critter *c: _critters)
        score += .5 * c->usableEnergy() / c->maxUsableEnergy();
  }

  return score;
}

} // end of namespace simu
