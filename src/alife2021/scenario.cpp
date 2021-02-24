#include "scenario.h"

namespace simu {

static constexpr auto R = Critter::MAX_SIZE * Critter::RADIUS;
static constexpr float fPI = M_PI;
static constexpr float TARGET_ENERGY = .85;

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
    utils::doThrow<std::invalid_argument>("Only V0 scenario use 4 coordinates");
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
    utils::doThrow<std::invalid_argument>("Only V1 scenario use 1 coordinate");
  }
  return s;
}

Scenario::Specs Scenario::Specs::fromString(const std::string &s) {
  std::ostringstream oss;
  auto type = Type((s[0] - '0') * V1_ALONE + (s[1] - '0'));
  if (type < V0_ALONE || V1_BOTH < type)
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

  } else
    spec = fromValue(type, toInt(s[2]));

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
  } else {
    oss << "1" << (s.type - V0_PREDATOR - 1) << sgn(s.food.y);
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
    g.gdata.self.gid = phylogeny::GID(-1);
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

genotype::Environment Scenario::environmentGenome (Specs::Type t) {
  genotype::Environment e;
  e.maxVegetalPortion = 1;
  e.taurus = 0;
  switch (t) {
  case Specs::V0_ALONE:
  case Specs::V0_CLONE:
  case Specs::V0_PREDATOR:
    e.size = 20;  break;
  case Specs::V1_ALONE:
  case Specs::V1_CLONE:
  case Specs::V1_PREDATOR:
  case Specs::V1_BOTH:
    e.size = 10;  break;
  }

  return e;
}

// =============================================================================

Scenario::Scenario (const std::string &specs, Simulation &simulation)
  : Scenario(Specs::fromString(specs), simulation) {}

Scenario::Scenario (const Specs &specs, Simulation &simulation)
  : _specs(specs), _simulation(simulation), _subject(nullptr) {
  simulation.setupCallbacks({
    { Simulation::POST_STEP, [this] { postStep(); } },
  });
}

void Scenario::init(const Genome &genome) {
  static constexpr auto E = Critter::maximalEnergyStorage(Critter::MAX_SIZE);
  _simulation.init(environmentGenome(_specs.type), {}, commonInitData);

  float S = _simulation.environment().extent();


  if (_specs.type <= Specs::V0_PREDATOR) {
    _subject = _simulation.addCritter(genome, 0, 0, 0, E, .5);
    _foodlet = _simulation.addFoodlet(BodyType::PLANT,
                                      _specs.food.x * .5 * S,
                                      _specs.food.y * .5 * S,
                                      Critter::RADIUS, E);

    if (_specs.type != Specs::V0_ALONE) {
      const Simulation::CGenome &g =
        _specs.type == Specs::V0_CLONE ? genome : predatorGenome();
      const P2D pos =
        _specs.type == Specs::V0_CLONE ? _specs.clone : _specs.predator;

      auto c = _simulation.addCritter(
        g, pos.x * (.5 * S - 3*R), pos.y * S,
        -pos.y * M_PI / 2., E, .5);

      if (_specs.type == Specs::V0_CLONE) {
        c->brainDead = false;
        c->selectiveBrainDead = {1,1,1,0,0};
      } else
        c->brainDead = true;

      _others.push_back(InstrumentalizedCritter(c));
      motors(c, 1, 1);
    }

  } else {
    _subject = _simulation.addCritter(genome, -S+R, 0, 0, E, .5);
    _foodlet = _simulation.addFoodlet(BodyType::PLANT,
                                      .5 * S, _specs.food.y * .5 * S,
                                      Critter::RADIUS, E);

    if (_specs.type == Specs::V1_CLONE || _specs.type == Specs::V1_BOTH) {
      auto c = _simulation.addCritter(
                genome,
                _specs.clone.x * (.5 * S - 3*R),
                _specs.clone.y * (S - 3 * R),
                -_specs.clone.y * M_PI / 2., E, .5);

      c->brainDead = false;
      c->selectiveBrainDead = {1,1,1,0,0};

      _others.push_back(InstrumentalizedCritter(c));
      motors(c, 1, 1);
    }

    if (_specs.type == Specs::V1_PREDATOR || _specs.type == Specs::V1_BOTH) {
      auto c = _simulation.addCritter(
                predatorGenome(),
                _specs.predator.x * (.5 * S - 3*R),
                _specs.predator.y * (S - 3 * R),
                -_specs.predator.y * M_PI / 2., E, .5);
      c->brainDead = true;

      _others.push_back(InstrumentalizedCritter(c));
      motors(c, 1, 1);
    }

    float width = .5, space = 1.5, length = S-space;
    _simulation.addObstacle(0, -.5*width, length, width);
  }
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

void Scenario::postStep(void) {
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
    bool hearing = predator && std::any_of(e.begin(), e.end(), [] (float v) {
      return v != 0;
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
        em[i/E] = std::max(em[i/E], e[i]);
      if (debugAI)  std::cerr << "hearing: " << em[0] << ", " << em[1] << "\n";
      float s = em[0]+em[1];
      MO o {em[1]/s, em[0]/s};
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
  assert(_simulation.finished());
  float e = (_subject->usableEnergy() / _subject->maxUsableEnergy());
  e = std::max(0.f, (e - TARGET_ENERGY) / (1 - TARGET_ENERGY));
  assert(0 <= e && e <= 1);

  if (predatorWon())  return -e;

  if (!isSubjectFeeding()) {
    float S = _simulation.environment().extent();
    const P2D oppositeCorner (-_specs.food.x * S, -_specs.food.y * S);
    const P2D fpos = (*_simulation._foodlets.begin())->pos();
    float distance = b2Distance(_subject->pos(), fpos),
          maxDistance = b2Distance(fpos, oppositeCorner);
    return distance / maxDistance;

  } else
    return 9*e+1;
}

} // end of namespace simu
