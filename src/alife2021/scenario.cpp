#include "scenario.h"

namespace simu {

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

const Scenario::Genome& Scenario::predator (void) {
  static const auto g = [] {
    rng::FastDice dice (0);
    auto g = Scenario::Genome::random(dice);
    g.cdata.sex = Critter::Sex::FEMALE;
    g.colors[0] = Color{1,0,0};
    g.colors[1] = Color{0,1,0};
    g.minClockSpeed = g.maxClockSpeed = .75;
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
  : _specs(specs) {
  simulation.setupCallbacks({
    { Simulation::POST_STEP, [this] (Simulation &s) { postStep(s); } },
  });
}

void Scenario::init(Simulation &s, const Genome &genome) {
  static constexpr auto E = Critter::maximalEnergyStorage(Critter::MAX_SIZE);
  static constexpr auto R = Critter::RADIUS * Critter::MAX_SIZE;
  s.addCritter(genome, 0, 0, 0, E, .5);

  float S = .5*s.environment().extent();
  s.addFoodlet(BodyType::PLANT, _specs.food.x * S, _specs.food.y * S,
               Critter::RADIUS, E);

  if (_specs.type != Specs::ALONE) {
    const Simulation::CGenome &g =
      _specs.type == Specs::CLONE ? genome : predator();
    auto c = s.addCritter(g, _specs.other.x * (S - 2*R), _specs.other.y * S,
                          -_specs.other.y * M_PI / 2., E, .5);
    c->brainDead = true;
  }
}

void Scenario::postStep(Simulation &s) {
  if (_specs.type != Specs::ALONE) {
    Critter *other = *s._critters.rbegin();
    other->setMotorOutput(1, Motor::LEFT);
    other->setMotorOutput(1, Motor::RIGHT);
  }
}

} // end of namespace simu
