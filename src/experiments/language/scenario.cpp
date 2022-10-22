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

struct {
  struct Size { float w,h; };
  Size eval {50,50};
  Size evo {10,5};
} ENV_SIZES;

genotype::Environment Scenario::environmentGenome (bool eval) {
  genotype::Environment e;
  e.maxVegetalPortion = 1;
  e.taurus = 0;

  if (eval) e.width = e.height = ENV_SIZES.eval.w;
  else      e.width = 2*(e.height = ENV_SIZES.evo.h);

  return e;
}

// =============================================================================

std::ostream& operator<< (std::ostream &os, const Scenario::Spec &s) {
  os << s.name;

  os << " t=" << s.target;

  os << " colors: {";
  for (const Color &c: s.colors) {
    os << " ";
    if (c == Color{{1,0,0}}) os << "R";
    else if (c == Color{{0,1,0}}) os << "G";
    else if (c == Color{{0,0,1}})  os << "B";
    else os << "?";
  }
  os << " }";

  if (!s.arg.empty())
    os << " [" << s.arg << "]";
  return os;
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

float critterXPos (int id) {  return (1-2*id);  }
float critterYPos (void) {    return 0;         }

Critter* Scenario::makeCritter (uint id, const genotype::Critter &genome,
                                const phenotype::ANN *brainTemplate) {

  static constexpr auto E = INFINITY;

  const auto pin = [] (auto c) {
    c->immobile = true;
//    c->paralyzed = true;
//    c->body().SetType(b2_staticBody);
  };

  auto c = _simulation.addCritter(genome, critterXPos(id), critterYPos(),
                                  id*M_PI, E, .5, true,
                                  brainTemplate);
  _critters.push_back(c);
  if (id == 0 || neuralEvaluation()) pin(c);  // emitter cannot move

//  if (team == 0 && id == 0)  c->mute = true;

  return c;
}

Foodlet* Scenario::makeFoodlet (float x, int side, Color c) {
  const float W = .5 * ENV_SIZES.evo.w,
              H = .5 * ENV_SIZES.evo.h;

  float fr = .25, fe = Foodlet::maxStorage(BodyType::PLANT, fr);
  auto fy = -2 * side * (x-.5f) * std::min(H-fr, 1 * H);

  auto f = _simulation.addFoodlet(BodyType::PLANT,
                                  side * (W-fr), fy, fr, fe);
  f->setBaseColor(c);
  _foodlets.push_back(f);

  return f;
}

void Scenario::muteReceiver(void) {
  receiver()->mute = true;
}

void Scenario::init(const Params &params) {
  _params = params;

  if (neuralEvaluation())
    std::cout << "Using flags: " << params.flags << "\n"
              << "Spec: " << params.spec << "\n";

  _simulation.init(environmentGenome(neuralEvaluation()),
                   {}, commonInitData);

  // Deactivate energy monitoring
  if (neuralEvaluation()) _simulation._systemExpectedEnergy = -1;

  if (neuralEvaluation()) {
    makeCritter(0, params.genome, params.brainTemplate);

    if (hasFlag(Params::VISION_EMITTER))
      makeFoodlet(.5, 1, params.spec.colors.front());

    else if (hasFlag(Params::VISION_RECEIVER)) {
      float n = params.spec.colors.size();
      for (uint i=0; i<n; i++)
        makeFoodlet(i/(n-1), 1, params.spec.colors[i]);

    } else if (hasAuditionFlag()) {
      std::ifstream ifs (_params.spec.arg);
      if (!ifs)
        utils::Thrower("Failed to open '", _params.spec.arg, "'");
      std::string line;
      std::getline(ifs, line);

      static constexpr auto EMITTER_COLUMN_HEADER = "EO0";
      auto headers = utils::split(line, ' ');
      auto it = std::find(headers.begin(), headers.end(), EMITTER_COLUMN_HEADER);
      if (it == headers.end())
        utils::Thrower("Failed to find column ", EMITTER_COLUMN_HEADER);
      uint col = std::distance(headers.begin(), it);

      auto data = _injectionData;
      while (std::getline(ifs, line)) {
        std::stringstream ss (utils::split(line, ' ')[col]);
        float val;
        ss >> val;
        data.push_back(val);
      }

      std::cerr << "Extracted data:\n  [";
      for (float v: data) std::cerr << " " << v;
      std::cerr << "]\n";

      const auto LOOP_TIMESTEPS =
          EVAL_STEP_DURATION * config::Simulation::ticksPerSecond() - 1;

      uint il, ir;
      for (il = 0; il<data.size() && data[il] == 0; il++);
      for (ir = data.size()-1;
           ir < data.size() && (data[ir] == 0 || ir>LOOP_TIMESTEPS);
           ir--);

      std::cerr << "Limiting auditive sample to [" << il << ":" << ir << "]\n";
      _injectionData = decltype(data)(data.begin()+il, data.begin()+ir+1);
      _injectionData.push_back(0);  // Pad with a zero

      std::cerr << "Target sample:\n  [";
      for (float v: _injectionData) std::cerr << " " << v;
      std::cerr << "]\n";
    }

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
      float n = params.spec.colors.size() - 1; // ignore rhs color
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
  static const auto TPS = config::Simulation::ticksPerSecond();
  static const auto TIMEOUT = DURATION * TPS;
  static const auto EVAL_PERIOD = EVAL_STEP_DURATION * TPS;
  const auto t = _simulation.currTime().timestamp();

  _mute &= emitter()->silent(false);

  if (neuralEvaluation()) {
    const auto step = t / EVAL_PERIOD;
    if (step >= EVAL_STEPS)  _simulation._finished = true;

    bool neutral = (step%2);
    Critter *sbj = *_critters.begin();

    // ensure it stays alive
    static const auto E = sbj->maximalEnergyStorage(Critter::MAX_SIZE);
    sbj->overrideUsableEnergyStorage(E);

    if (!neutral && hasAuditionFlag()) {
      auto &ears = sbj->ears();
      ears.fill(0);
      float val = 0;
      val = _injectionData[(t-1) % _injectionData.size()];
      ears[1] = ears[3] = val;
    }

    if (!((t-1) % EVAL_PERIOD == 0)) return;

    if (hasVisionFlag()) {
      static constexpr Color BLACK {0,0,0};

      for (uint i=0; i<_foodlets.size(); i++)
        _foodlets[i]->setBaseColor(neutral ? BLACK : _params.spec.colors[i]);
    }

    using F = Params::Flag;
    for (Params::Flag f: {F::VISION_EMITTER, F::VISION_RECEIVER,
                          F::AUDITION_0, F::AUDITION_1,F::AUDITION_2})
      if (hasFlag(f)) _currentFlags.flip(f);

  } else {
    _simulation._finished =
         (t >= TIMEOUT)
      || !receiver()->body().IsAwake()
      || (!_simulation.environment().feedingEvents().empty());
  }

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
      const auto vdist = [this] (uint id, const b2Vec2 &pos) {
        return (_foodlets[id]->body().GetWorldCenter() - pos).Length();
      };

      const auto cdist = [this, vdist] (uint id, const simu::Critter *c) {
        return vdist(id, c->pos());
      };

      int target = _params.spec.target;
      if (_params.type == Type::COLOR) { // Must manually look for the target
        target = -1;
        for (uint i=0; i<_foodlets.size()-1; i++)
          if (_foodlets[i]->baseColor() == _params.spec.colors.back())
            target = i;
      }

      if (target >= 0)
        score += 1 - cdist(target, receiver())
                     / vdist(target, {critterXPos(1), critterYPos()});

//      if (target >= 0) {
//        std::cerr << "FITNESS IS UNDEREVALUATED\n";
//        score += 1 - cdist(target, receiver())
//                    / cdist(_foodlets.size()-1, emitter());
//      }

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
