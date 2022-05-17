#include "scenario.h"

namespace simu {

// =============================================================================

Scenario::Spec Scenario::Spec::fromString(const std::string &s) {
  Spec spec;

  spec.type = ERROR;
  switch (s[0]) {
  case 'd': spec.type = DIRECTION; break;
  case 'c': spec.type = COLOR; break;
  default:
    utils::Thrower("Unknown scenario type ", s[0]);
  }

  if (spec.type == DIRECTION) {
    switch (s[2]) {
    case 'l': spec.target = 0; break;
    case 'f': spec.target = 1; break;
    case 'r': spec.target = 2; break;
    default:
      utils::Thrower("Unknown scenario direction subtype ", s[2]);
    }
  } else if (spec.type == COLOR) {
    std::cerr << "Color spec is brick-implemented\n";
    spec.target = 1;
    spec.colors = {{ {1,0,0}, {0,1,0}, {0,0,1} }};
  }

  return spec;
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

genotype::Environment Scenario::environmentGenome (bool eval) {
  genotype::Environment e;
  e.maxVegetalPortion = 1;
  e.taurus = 0;

  if (eval) e.width = e.height = 50;
  else      e.width = 2*(e.height = 3);

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

Critter* Scenario::makeCritter (uint id, const genotype::Critter &genome) {

  static constexpr auto E = INFINITY;
  static constexpr auto R = Critter::MAX_SIZE;
  static constexpr auto D = 1;

  static const auto x = [this] (int id) {
    return (1-2*id)
        * _simulation.environment().xextent() / 3.f;
  };
  static const auto y = [] (int i, int teamSize) {
    if (teamSize == 1)  return 0.f;
    else                return D*R*(2*i-1);
  };
  static const auto pin = [] (auto c) {
    c->immobile = true;
//    c->paralyzed = true;
//    c->body().SetType(b2_staticBody);
  };

  auto c = _simulation.addCritter(genome, x(id), 0,
                                  id*M_PI, E, .5, true);
  _critters.push_back(c);
  if (id == 0 || neuralEvaluation()) pin(c);  // emitter cannot move

  std::cerr << "Pinning everyone\n";
  pin(c);

//  if (team == 0 && id == 0)  c->mute = true;

  return c;
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
      makeCritter(i, params.genome);

    float W = _simulation.environment().xextent(),
          H = _simulation.environment().yextent();

    float width = .25;
    _simulation.addObstacle(-.5*width, -H, width, 2*H);

    if (params.spec.type == Scenario::Spec::DIRECTION) {
      float fr = .25, fe = Foodlet::maxStorage(BodyType::PLANT, fr);
      const auto fy =
          [H, fr] (uint i) { return (1.f-i) * std::min(H-fr, 1 * H); };

      _simulation.addFoodlet(BodyType::PLANT,
                             W-fr, fy(params.spec.target),
                             fr, fe);

      for (uint i=0; i<3; i++)
        _simulation.addFoodlet(BodyType::PLANT, -W+fr, fy(i), fr, fe);

    } else if (params.spec.type == Scenario::Spec::COLOR) {
      float width = .25, length = 2/3.f;
      _simulation.addObstacle(W-width, -.5*length, width, length,
                              Color{1,0,0});

      for (uint i=0; i<3; i++)
        _simulation.addObstacle(-W, 1.f-i-.5*length,
                                width, length,
                                Color{i==0,i==1,i==2});
    }
  }

  std::set<phylogeny::GID> gids;
  for (auto &c: _critters) {
    auto r = gids.insert(c->id());
    assert(r.second);
    (void)r;
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

char Scenario::critterRole(uint id) {
  switch (id) {
  case 0: return 'e';
  case 1: return 'r';
  case 2: return 'a';
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

//  static const auto damage = [] (simu::Critter *c, bool damage, bool allSplines) {
//    decimal h = 1 - damage * INJURY;
//    c->overrideBodyHealthness(h);
//    if (allSplines)
//      for (Critter::Side s: {Critter::Side::LEFT, Critter::Side::RIGHT})
//        for (uint i=0; i<Critter::SPLINES_COUNT; i++)
//          c->overrideSplineHealthness(h, i, s);
//  };
//  static const auto injectSound = [] (simu::Critter *c, uint channel) {
//    auto &ears = c->ears();
//    ears.fill(0);
//    ears[channel] = ears[channel+Critter::VOCAL_CHANNELS+1] = 1;
//  };

//  if (neuralEvaluation()) {
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
//  }

//  bool casualty = (_teams[0].size() < _teamsSize)
//               || (_teams[1].size() < _teamsSize);

//  bool awake = false;
//  for (const auto &team: _teams) {
//    for (const Critter *c: team) {
//      awake |= c->body().IsAwake();
//      if (awake)  break;
//    }
//    if (awake)  break;
//  }

  _simulation._finished = (t >= TIMEOUT);
}

void Scenario::preDelCritter(Critter *c) {(void)c;}

float Scenario::score (void) const {
  if (_simulation._aborted) return -std::numeric_limits<float>::max();

  float score = 0;
  return score;
}

} // end of namespace simu
