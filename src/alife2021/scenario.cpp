#include "scenario.h"

namespace simu {

static constexpr auto R = Critter::MAX_SIZE * Critter::RADIUS;
static constexpr float fPI = M_PI;
static constexpr float TARGET_ENERGY = .75;

static constexpr bool debugAI = false;

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
const Scenario::Genome& Scenario::predator (void) {
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
    return g;
  }();
  return g;
}

Scenario::Specs Scenario::Specs::fromString(const std::string &s) {
  std::ostringstream oss;
  Scenario::Specs ss;
  ss.type = Type(s[0] - '0');
  if (ss.type < ALONE || PREDATOR < ss.type)
    oss << "Type '" << s[0] << "' is not recognized";

  if (ss.type == ALONE && s.size() != 3)
    oss << "Invalid number of specifications for type ALONE";
  else if (ss.type != ALONE && s.size() != 5)
    oss << "Invalid number of specifications for scenario with two splinoids";

  const auto toInt = [&oss] (char c) {
    if (c != '+' && c != '-') {
      oss << "Invalid specification '" << c << "'!";
      return 0;
    } else
      return c == '+' ? +1 : -1;
  };
  if (oss.str().empty()) {
    ss.food.x = toInt(s[1]);
    ss.food.y = toInt(s[2]);
    if (s.size() > 3) {
      ss.other.x = toInt(s[3]);
      ss.other.y = toInt(s[4]);
    }
  }

  if (!oss.str().empty())
    utils::doThrow<std::invalid_argument>(
      "Failed to parse '", s, "' as a scenario specification: ", oss.str());
  return ss;
}

Scenario::Scenario (const std::string &specs, Simulation &simulation)
  : Scenario(Specs::fromString(specs), simulation) {}

Scenario::Scenario (const Specs &specs, Simulation &simulation)
  : _specs(specs), _simulation(simulation), _subject(nullptr), _other(nullptr) {
  simulation.setupCallbacks({
    { Simulation::POST_STEP, [this] { postStep(); } },
  });

  _pangle = NAN;
  _collisionDelay = 0;
}

void Scenario::init(const Genome &genome) {
  static constexpr auto E = Critter::maximalEnergyStorage(Critter::MAX_SIZE);
  auto modifiedGenome = genome; /// TODO Remove
  modifiedGenome.colors[0] = config::Simulation::obstacleColor();
  _subject = _simulation.addCritter(modifiedGenome, 0, 0, 0, E, .5);

  float S = .5*_simulation.environment().extent();
  _simulation.addFoodlet(BodyType::PLANT, _specs.food.x * S, _specs.food.y * S,
                         Critter::RADIUS, E);

  if (_specs.type != Specs::ALONE) {
    const Simulation::CGenome &g =
      _specs.type == Specs::CLONE ? genome : predator();
    _other = _simulation.addCritter(
      g, _specs.other.x * (S - 3*R), _specs.other.y * S,
      -_specs.other.y * M_PI / 2., E, .5);
    _other->brainDead = true;
    motors(1,1);
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

void Scenario::postStep(void) {
  if (_specs.type != Specs::ALONE) {
    bool predator = (_specs.type == Specs::PREDATOR);
    float y = _other->pos().y, a = _other->rotation();

    auto &body = _other->body();

    const auto &r = _other->retina();
    assert(r.size() == PREDATOR_RETINA_SIZE);
    using MO = MotorOutput;
    static constexpr std::array<MO, PREDATOR_RETINA_SIZE> visionResponse {{
      {0,1}, {1,1}, {1,0},  {0,1}, {1,1}, {1,0}
    }};
    bool seeing = predator && std::any_of(r.begin(), r.end(), &splinoid);

    const auto &e = _other->ears();
    bool hearing = predator && std::any_of(e.begin(), e.end(), [] (float v) {
      return v != 0;
    });

    bool collision = nonSensorCollision(body);

    if (debugAI)
      std::cerr << CID(_other) << " " << y << " "
                << 180. * a / fPI << " "
                << body.GetLinearVelocity() << " "
                << body.GetAngularVelocity() << " "
                << seeing << " " << hearing << " "
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
      motors(o);

    } else if (hearing) {
      static constexpr auto E = Critter::VOCAL_CHANNELS+1;
      std::array<float, 2> em {0};
      for (uint i=0; i<e.size(); i++)
        em[i/E] = std::max(em[i/E], e[i]);
      if (debugAI)  std::cerr << "hearing: " << em[0] << ", " << em[1] << "\n";
      float s = em[0]+em[1];
      MO o {em[1]/s, em[0]/s};
      motors(o);
      if (debugAI)  std::cerr << "\tapplying " << o.l << "," << o.r;

    } else if (std::isnan(_pangle) && collision) { // collision
      if (_collisionDelay <= 0) {
        if (debugAI)  std::cerr << "collision, new target: ";
        _pangle = a+fPI;
        if (debugAI)  std::cerr << _pangle;
      } else {
        if (debugAI)  std::cerr << "Ignoring collision";
        _collisionDelay--;
      }

    } else if (!std::isnan(_pangle)) { // rotating
      if (debugAI)  std::cerr << "rotating\t";
      if (a < _pangle) {
        if (debugAI)  std::cerr << "target: " << 180. * _pangle / fPI;
        motors(-1, 1);

      } else {
        if (debugAI)  std::cerr << "handbreak";
        body.SetAngularVelocity(0);
        body.SetTransform(body.GetPosition(), positiveAngle(_pangle));
        motors(1,1);
        disableContacts(body);
        _pangle = NAN;
        _collisionDelay = 2;
      }

    } else {
      motors(1, 1);
      if (debugAI)  std::cerr << "forward";
    }

    if (debugAI)  std::cerr << "\n";

    _simulation._finished =
      (_specs.type == Specs::PREDATOR
      && !_simulation.environment().fightingEvents().empty());
  }


  _simulation._finished |= isSubjectFeeding();

  _simulation._finished |=
    (_subject->usableEnergy() / _subject->maxUsableEnergy()) <= TARGET_ENERGY;
}

float Scenario::score (void) const {
  assert(_simulation.finished());
  float e = (_subject->usableEnergy() / _subject->maxUsableEnergy());
  e = std::max(0.f, (e - TARGET_ENERGY) / (1 - TARGET_ENERGY));
  assert(0 <= e && e <= 1);

  if (_specs.type == Specs::PREDATOR
      && !_simulation.environment().fightingEvents().empty())
    return -e;

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
