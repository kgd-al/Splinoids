#include "scenario.h"

namespace simu {

static constexpr auto R = Critter::MAX_SIZE * Critter::RADIUS;
static constexpr float fPI = M_PI;
static constexpr float TARGET_ENERGY = .90;

static constexpr bool debugAI = false;

static const P2D nanPos {NAN,NAN};

Scenario::Specs
Scenario::Specs::fromValues(Type t, int fx, int fy, int ox, int oy) {
  Scenario::Specs s;
  s.type = t;
  s.food.x = fx;
  s.food.y = fy;
  s.predator = nanPos;
  s.clone = nanPos;
  switch (t) {
  case V0_ALONE:  break;
  case V0_CLONE:  s.clone.x = ox; s.clone.y = oy; break;
  case V0_PREDATOR: s.predator.x = ox; s.predator.y = oy; break;
  default:
    utils::doThrow<std::invalid_argument>(
      "Only V0 scenario use 4 coordinates (t=", t, ")");
  }
  return s;
}

Scenario::Specs
Scenario::Specs::fromValue(Type t, int y) {
  Scenario::Specs s;
  s.type = t;
  s.food = {1, float(y)};
  s.predator = {1, NAN};
  s.clone = {1, NAN};
  switch (t) {
  case V1_ALONE:  break;
  case V1_CLONE:  s.clone.y = y;  break;
  case V1_PREDATOR: s.predator.y = y; break;
  case V1_BOTH: s.predator.y = -(s.clone.y = y);  break;
  default:
    utils::doThrow<std::invalid_argument>(
      "Only V1 scenarios use 1 coordinate (t=", t, ")");
  }
  return s;
}

Scenario::Specs Scenario::Specs::fromString(const std::string &s) {
  std::ostringstream oss;
  if (s.size() < 2)
    utils::doThrow<std::invalid_argument>(
      "Argument '", s, "' is too small to be a scenario specification");

  Type type = ERROR;
  if (s[0] == '0' || s[0] == '1')
    type = Type((s[0] - '0') * V1_ALONE + (s[1] - '0'));
  else if (s[0] == 'E')
    type = EVAL;
  if (type == ERROR)
    oss << "Type '" << s[0] << s[1] << "' is not recognized";

  if (type == V0_ALONE && s.size() != 4)
    oss << "Invalid number of specifications for type ALONE";
  else if ((type == V0_CLONE || type == V0_PREDATOR) && s.size() != 6)
    oss << "Invalid number of specifications for scenario with two splinoids";
  else if (V1_ALONE <= type && type <= V1_BOTH && s.size() != 3)
    oss << "Invalid number of specifications for v1 scenario ([0-9][+-])";

  const auto toInt = [&oss] (char c) {
    if (c != '+' && c != '-') {
      oss << "Invalid specification '" << c << "'!";
      return 0;
    } else
      return c == '+' ? +1 : -1;
  };

  Specs spec;
  spec.type = type;

  if (oss.str().empty()) {
    if (type <= V0_PREDATOR) {
      int fx = 0, fy = 0, ox = 0, oy = 0;
      if (oss.str().empty()) {
        fx = toInt(s[2]);
        fy = toInt(s[3]);
        if (s.size() > 4) {
          ox = toInt(s[4]);
          oy = toInt(s[5]);
        }
      }

      spec = fromValues(type, fx, fy, ox, oy);

    } else if (type <= V1_BOTH)
      spec = fromValue(type, toInt(s[2]));

    else {
      P2D pos {1,0};
      spec.food = spec.clone = spec.predator = nanPos;
      switch (s[1]) {
      case 'E': spec.predator = pos;  break;
      case 'A': spec.clone = pos;  break;
      case 'G': spec.food = pos;  break;
      default:  break;
      }

      Type stype = ERROR;
      switch (s[1]) {
      case 'E': stype = EVAL_PREDATOR;  break;
      case 'A': stype = EVAL_CLONE;  break;
      case 'G': stype = EVAL_FOOD;  break;
      case 'B': stype = Type(uint(EVAL_PREDATOR)|uint(EVAL_CLONE)); break;
      default:
        break;
      }

      if (s.size() == 3) {
        if (s[2] == 'I')
          stype = Type(uint(stype) | uint(EVAL_CLONE_INVISIBLE));
        else if (s[2] == 'M')
          stype = Type(uint(stype) | uint(EVAL_CLONE_MUTE));
      }

      spec.type = Type(uint(spec.type) | uint(stype));
    }
  }

  if (!oss.str().empty())
    utils::doThrow<std::invalid_argument>(
      "Failed to parse '", s, "' as a scenario specification: ", oss.str());

  return spec;
}

std::string Scenario::Specs::toString(const Specs &s) {
  static const auto sgn =
    [] (float x) { return x < 0 ? '-' : x > 0 ? '+' : '!'; };
  std::ostringstream oss;
  if (s.type <= V0_PREDATOR) {
    oss << "0"<< s.type;
    oss << sgn(s.food.x) << sgn(s.food.y);
    if (s.type == V0_CLONE)  oss << sgn(s.clone.x) << sgn(s.clone.y);
    if (s.type == V0_PREDATOR)  oss << sgn(s.predator.x) << sgn(s.predator.y);

  } else if (s.type <= V1_BOTH){
    oss << "1" << (s.type - V0_PREDATOR - 1) << sgn(s.food.y);

  } else {
    oss << "E";
    if (s.type & EVAL_PREDATOR && s.type & EVAL_CLONE)
      oss << "B";
    else if (s.type & EVAL_PREDATOR)
      oss << "E";
    else if (s.type & EVAL_CLONE)
      oss << "A";
    else if (s.type & EVAL_FOOD)
      oss << "G";
    else
      oss << "?";

    if (s.type & EVAL_CLONE_INVISIBLE)  oss << "I";
    if (s.type & EVAL_CLONE_MUTE)  oss << "M";
  }
  return oss.str();
}

float Scenario::Specs::difficulty(const Specs &s) {
  float d = std::fabs(atan2(s.food.y, s.food.x) / M_PI);
  if (s.type != V0_ALONE) {

  }
  return d;
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

static constexpr uint PREDATOR_VISION_PRECISION = 1;
static constexpr uint PREDATOR_RETINA_SIZE = 2*(2*PREDATOR_VISION_PRECISION+1);
const Scenario::Genome& Scenario::predatorGenome (void) {
  static const auto g = [] {
    rng::FastDice dice (0);
    auto g = Scenario::Genome::random(dice);
    g.gdata.self.gid = phylogeny::GID(2);
    g.cdata.sex = Critter::Sex::FEMALE;
    g.colors[0] = Color{1,0,0};
    g.colors[1] = Color{0,1,0};
    g.minClockSpeed = g.maxClockSpeed = .75;
    g.vision.angleBody = M_PI/8.f;
    g.vision.angleRelative = -g.vision.angleBody;
    g.vision.width = g.vision.width / 2;
    g.vision.precision = PREDATOR_VISION_PRECISION;
    g.brain.cppn.nodes.clear();
    g.brain.cppn.links.clear();
    return g;
  }();
  return g;
}

Scenario::Genome Scenario::cloneGenome (Scenario::Genome g) {
  g.gdata.self.gid = phylogeny::GID(1);
  return g;
}

genotype::Environment Scenario::environmentGenome (Specs::Type t) {
  genotype::Environment e;
  e.maxVegetalPortion = 1;
  e.taurus = 0;
  switch (t) {
  case Specs::V0_ALONE:
  case Specs::V0_CLONE:
  case Specs::V0_PREDATOR:
    e.width = e.height = 20;
    break;
  case Specs::V1_ALONE:
  case Specs::V1_CLONE:
  case Specs::V1_PREDATOR:
  case Specs::V1_BOTH:
    e.width = 10. * (1 + sqrt(5)) / 2;
    e.height = 10;
    break;
  default:
    e.width = e.height = 100;
  }

  return e;
}

// =============================================================================

Scenario::Scenario (const std::string &specs, Simulation &simulation)
  : Scenario(Specs::fromString(specs), simulation) {}

Scenario::Scenario (const Specs &specs, Simulation &simulation)
  : _specs(specs), _simulation(simulation),
    _foodlet(nullptr), _subject(nullptr) {
  simulation.setupCallbacks({
    { Simulation::POST_ENV_STEP, [this] { postEnvStep(); } },
    {     Simulation::POST_STEP, [this] { postStep();    } },
  });
}

void Scenario::init(Genome genome) {
  static constexpr auto E = Critter::maximalEnergyStorage(Critter::MAX_SIZE);
  _simulation.init(environmentGenome(_specs.type), {}, commonInitData);

  float W = _simulation.environment().xextent(),
        H = _simulation.environment().yextent();

  genome.gdata.self.gid = phylogeny::GID(0);
  if (_specs.type <= Specs::V0_PREDATOR) {
    _subject = _simulation.addCritter(genome, 0, 0, 0, E, .5);
    _foodlet = _simulation.addFoodlet(BodyType::PLANT,
                                      _specs.food.x * .5 * W,
                                      _specs.food.y * .5 * H,
                                      Critter::RADIUS, E);

    if (_specs.type != Specs::V0_ALONE) {
      Simulation::CGenome g;
      if (_specs.type == Specs::V0_CLONE) {
        g = genome;
        g.gdata.self.gid = phylogeny::GID(1);

      } else
        g = predatorGenome();

      const P2D pos =
        _specs.type == Specs::V0_CLONE ? _specs.clone : _specs.predator;

      auto c = _simulation.addCritter(
        g, pos.x * (.5 * W - 3*R), pos.y * H,
        -pos.y * M_PI / 2., E, .5);

      if (_specs.type == Specs::V0_CLONE) {
        c->brainDead = false;
        c->selectiveBrainDead = {1,1,1,0,0};
      } else
        c->brainDead = true;

      _others.push_back(InstrumentalizedCritter(c));
      motors(c, 1, 1);
    }

  } else if (_specs.type <= Specs::V1_BOTH) {
    _subject = _simulation.addCritter(genome, -W+R, 0, 0, E, .5);
    _foodlet = _simulation.addFoodlet(BodyType::PLANT,
                                      .5 * W, _specs.food.y * .5 * H,
                                      Critter::RADIUS, E);

    if (_specs.type == Specs::V1_CLONE || _specs.type == Specs::V1_BOTH) {
      auto c = _simulation.addCritter(
                cloneGenome(genome),
                _specs.clone.x * (.5 * W - 3*R),
                _specs.clone.y * (H - 3 * R),
                -_specs.clone.y * M_PI / 2., E, .5);

      c->brainDead = false;
      c->selectiveBrainDead = {1,1,1,0,0};

      _others.push_back(InstrumentalizedCritter(c));
      motors(c, 1, 1);
    }

    if (_specs.type == Specs::V1_PREDATOR || _specs.type == Specs::V1_BOTH) {
      auto c = _simulation.addCritter(
                predatorGenome(),
                _specs.predator.x * (.5 * W - 3*R),
                _specs.predator.y * (H - 3 * R),
                -_specs.predator.y * M_PI / 2., E, .5);
      c->brainDead = true;

      _others.push_back(InstrumentalizedCritter(c));
      motors(c, 1, 1);
    }

    float width = .5, space = 1.5, length = W-space;
    _simulation.addObstacle(0, -.5*width, length, width);

  } else if (_specs.type & Specs::EVAL) {
    _simulation._systemExpectedEnergy = -1;
    _subject = _simulation.addCritter(genome, -1+R, 0, 0, E, .5);

    // Deactivate motion
    _subject->immobile = true;
  }
}

void Scenario::applyLesions(int lesions) {
  if (lesions == 0) return;
  std::cout << "Applying lesion type: " << lesions;

  uint count = 0;
  for (auto &ptr: _subject->brain().neurons()) {
    phenotype::ANN::Neuron &n = *ptr;
    for (auto it=n.links().begin(); it!=n.links().end(); ) {
      phenotype::ANN::Neuron &tgt = *(it->in.lock());
      if (tgt.type == phenotype::ANN::Neuron::I
          && (lesions == 3
            || (lesions == 1 && tgt.pos.y() < -.75)   // deactivate vision
            || (lesions == 2 && tgt.pos.y() >= -.75)  // deactivate audition
            || (n.flags == 1024 && ( // deactivate amygdala inputs
                 (lesions == 4 && tgt.pos.y() == -1)    // deactivate red
              || (lesions == 5 && tgt.pos.y() == -.75))))) { // deactivate noise audition
        it = n.links().erase(it);
        count++;
//        std::cerr << "Deactivated " << tgt.pos << " -> " << n.pos << "\n";
      } else {
        ++it;
//        std::cerr << "Kept " << tgt.pos << " -> " << n.pos << " (" << n.flags << ")\n";
      }
    }
  }

  std::cout << " (" << count << " links deleted)\n";
}

bool splinoid (const Color &c) {
  return c != config::Simulation::obstacleColor()
      && (c[0] != 0 || c[2] != 0);
}

float positiveAngle (float v) {
  while (v >  2*fPI)  v -= 2*fPI;
  while (v < -2*fPI)  v += 2*fPI;
  return v;
}

bool nonSensorCollision (const b2Body &b) {
  for (const b2ContactEdge* edge = b.GetContactList(); edge; edge = edge->next){
    if (edge->contact->IsTouching()
        && !edge->contact->GetFixtureA()->IsSensor()
        && !edge->contact->GetFixtureB()->IsSensor()) {
      if (debugAI) {
        std::cerr << "non sensor collision between ";
        if (auto a = Critter::get(edge->contact->GetFixtureA()))
          std::cerr << *a;
        else
          std::cerr << "?";
        std::cerr << " and ";
        if (auto b = Critter::get(edge->contact->GetFixtureB()))
          std::cerr << *b;
        else
          std::cerr << "?";
      }
      return true;
    }
  }
  return false;
}

void disableContacts(b2Body &b) {
  for (b2ContactEdge* edge = b.GetContactList(); edge; edge = edge->next)
    edge->contact->SetEnabled(false);
}

bool Scenario::isSubjectFeeding(void) const {
  for (const auto &e: _simulation.environment().feedingEvents())
    if (e.critter == _subject)
      return true;
  return false;
}

bool Scenario::predatorWon(void) const {
  const Critter *s = _subject, *p = predator();
  if (!p) return false;
  for (const auto &e: _simulation.environment().fightingEvents()) {
    auto l = e.first.first, r = e.first.second;
    if ( (l == s && r == p) || (l == p && r == s))  return true;
  }
  return false;
}

void Scenario::postEnvStep(void) {
  // Prevent predator from hearing clone
  if (_others.size() == 2)
    _simulation.environment().hearingEvents().erase(
        std::make_pair(_others[0].critter, _others[1].critter));
}

void Scenario::postStep(void) {
  if (_specs.type & Specs::EVAL) {
    static constexpr auto E = Critter::maximalEnergyStorage(Critter::MAX_SIZE);
    static constexpr auto PERIOD = 100;

    const auto makeCritter = [this] (bool predator) {
      auto g = predator ? predatorGenome() : cloneGenome(_subject->genotype());
      if (!predator && (_specs.type & Specs::EVAL_CLONE_INVISIBLE))
        g.colors.fill({0});

      auto c = _simulation.addCritter(g, 1 + R, 0, -M_PI, E, .5);

      c->brainDead = predator;
      c->immobile = true;
      c->mute = predator || (_specs.type & Specs::EVAL_CLONE_MUTE);
      c->setNoisy(predator);
      _others.push_back(InstrumentalizedCritter(c));
      motors(c, 0, 0);
      return c;
    };

    const auto t = _simulation.currTime().timestamp();
    if (t % PERIOD == 0) {
      const auto step = t / PERIOD;
//      std::cerr << "Update period: " << step << "\n";

      // Zaelous cleaning
      for (auto &c: _others)  _simulation.delCritter(c.critter);
      _others.clear();

      _simulation._finished = (step >= 6);
      if (_simulation._finished)  return;

      if (step%2 == 0) {
        if (_specs.type & Specs::EVAL_FOOD && _foodlet) {
          _simulation.delFoodlet(_foodlet);
          _foodlet = nullptr;
        }

        if ((_specs.type & Specs::EVAL_PREDATOR)
            && (_specs.type & Specs::EVAL_CLONE) && step > 0)
          makeCritter(false);

      } else {
        if (_specs.type & Specs::EVAL_PREDATOR)
          makeCritter(true);
        else if (_specs.type & Specs::EVAL_CLONE)
          makeCritter(false);
        else if (_specs.type & Specs::EVAL_FOOD)
          _foodlet = _simulation.addFoodlet(BodyType::PLANT,
                                            1 + R, 0, R, E);
      }
    }

    return;
  }

  for (uint i=0; i<_others.size(); i++) {
    bool predator = (_specs.type == Specs::V0_PREDATOR)
                    || (_specs.type == Specs::V1_PREDATOR)
                    || (i == 1);
    auto &ic = _others[i];
    auto c = ic.critter;
    float y = c->pos().y, a = c->rotation();

    auto &body = c->body();

    const auto &r = c->retina();
    assert(!predator || r.size() == PREDATOR_RETINA_SIZE);
    using MO = MotorOutput;
    static constexpr std::array<MO, PREDATOR_RETINA_SIZE> visionResponse {{
      {0,1}, {1,1}, {1,0},  {0,1}, {1,1}, {1,0}
    }};
    bool seeing = predator && std::any_of(r.begin(), r.end(), &splinoid);

    const auto &e = c->ears();
    static constexpr auto predatorHearingThreshold = .5;
    bool hearing = predator &&
        std::any_of(std::next(e.begin()), e.end(), [] (float v) {
          return v > predatorHearingThreshold;
        });

    bool collision = nonSensorCollision(body);

    if (debugAI)
      std::cerr << CID(c) << " " << y << " "
                << 180. * a / fPI << " "
                << body.GetLinearVelocity() << " "
                << body.GetAngularVelocity() << " "
                << predator << " " << seeing << " " << hearing << " "
                << collision << " "
                << "\n";

    if (debugAI)  std::cerr << "\t";
    if (seeing) {
      if (debugAI)  std::cerr << "seeing:";
      MO o {0,0};
      for (uint i=0; i<PREDATOR_RETINA_SIZE; i++) {
        if (debugAI)  std::cerr << " " << splinoid(r[i]);
        if (splinoid(r[i])) max(o, visionResponse[i]);
      }
      if (debugAI)  std::cerr << "\tapplying " << o.l << "," << o.r;
      motors(c, o);

    } else if (hearing) {
      static constexpr auto E = Critter::VOCAL_CHANNELS+1;
      std::array<float, 2> em {0};
      for (uint i=0; i<e.size(); i++)
        if (e[i] > predatorHearingThreshold)
          em[i/E] = std::max(em[i/E], e[i]);
      if (debugAI)  std::cerr << "hearing: " << em[0] << ", " << em[1] << "\n";
      MO o {em[1]-em[0], em[0]-em[1]};
      motors(c, o);
      if (debugAI)  std::cerr << "\tapplying " << o.l << "," << o.r;

    } else if (std::isnan(ic.pangle) && collision) { // collision
      if (ic.collisionDelay <= 0) {
        if (debugAI)  std::cerr << "collision, new target: ";
        ic.pangle = a+fPI;
        if (debugAI)  std::cerr << ic.pangle;
      } else {
        if (debugAI)  std::cerr << "Ignoring collision";
        ic.collisionDelay--;
      }

    } else if (!std::isnan(ic.pangle)) { // rotating
      if (debugAI)  std::cerr << "rotating\t";
      if (a < ic.pangle) {
        if (debugAI)  std::cerr << "target: " << 180. * ic.pangle / fPI;
        motors(c, -1, 1);

      } else {
        if (debugAI)  std::cerr << "handbreak";
        body.SetAngularVelocity(0);
        body.SetTransform(body.GetPosition(), positiveAngle(ic.pangle));
        motors(c, 1,1);
        disableContacts(body);
        ic.pangle = NAN;
        ic.collisionDelay = 2;
      }

    } else {
      motors(c, 1, 1);
      if (debugAI)  std::cerr << "forward";
    }

    if (debugAI)  std::cerr << "\n";

    _simulation._finished |= predatorWon();

    if (debugAI && predator && predatorWon())
      std::cerr << "Predator won!\n";
  }

  _simulation._finished |= isSubjectFeeding();

  _simulation._finished |=
    (_subject->usableEnergy() / _subject->maxUsableEnergy()) <= TARGET_ENERGY;
//  std::cerr << " " << _simulation.currTime().timestamp() << " "
//            << _subject->usableEnergy() / _subject->maxUsableEnergy() << "\n";

  if (debugAI && _simulation.finished())
    std::cerr << "Scenario completed\n";
}

float Scenario::score (void) const {
  if (_specs.type & Specs::EVAL)  return 0;

  assert(_simulation.finished());
  float e = (_subject->usableEnergy() / _subject->maxUsableEnergy());
  e = std::max(0.f, (e - TARGET_ENERGY) / (1 - TARGET_ENERGY));
  assert(0 <= e && e <= 1);

  if (predatorWon())  return -e;

  if (!isSubjectFeeding()) {
    float W = _simulation.environment().xextent(),
          H = _simulation.environment().yextent();
    const P2D oppositeCorner (-_specs.food.x * W, -_specs.food.y * H);
    const P2D fpos = (*_simulation._foodlets.begin())->pos();
    float distance = b2Distance(_subject->pos(), fpos),
          maxDistance = b2Distance(fpos, oppositeCorner);
//    std::cerr << "Food at " << fpos << ", opposite corner: "
//              << oppositeCorner << ", max distance: " << maxDistance
//              << ", distance: " << distance << "\n";
    return 1 - distance / maxDistance;

  } else
    return 9*e+1;
}

} // end of namespace simu
