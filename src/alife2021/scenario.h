#ifndef SCENARIO_H
#define SCENARIO_H

#include "../simu/simulation.h"

namespace simu {

class Scenario {
  using Genome = Simulation::CGenome;
public:  
  struct Specs {
    enum Type {
      V0_ALONE, V0_CLONE, V0_PREDATOR,
      V1_ALONE, V1_CLONE, V1_PREDATOR, V1_BOTH
    };
    Type type;
    P2D food, clone, predator;

    static Specs fromValues (Type t, int fx, int fy, int ox = 0, int oy = 0);
    static Specs fromValue (Type t, int y);

    static Specs fromString (const std::string &s);
    static std::string toString (const Specs &s);

    static float difficulty (const Specs &s);
  };

  Scenario(const Specs &specs, Simulation &simulation);
  Scenario(const std::string &specs, Simulation &simulation);

  void init (Genome genome, int lesions = 0);
  void postStep (void);

  const auto& specs (void) const {
    return _specs;
  }

  const auto foodlet (void) const {
    return _foodlet;
  }

  const auto subject (void) const {
    return _subject;
  }

  const Critter* clone (void) const {
    switch (_specs.type) {
    case Specs::V0_CLONE:
    case Specs::V1_CLONE:
    case Specs::V1_BOTH:
      return _others[0].critter;
    default:
      return nullptr;
    }
  }

  const Critter* predator (void) const {
    switch (_specs.type) {
    case Specs::V0_PREDATOR:
    case Specs::V1_PREDATOR:
      return _others[0].critter;
    case Specs::V1_BOTH:
      return _others[1].critter;
    default:
      return nullptr;
    }
  }

  float score (void) const;

  static const Simulation::InitData commonInitData;

private:
  const Specs _specs;
  Simulation &_simulation;

  Foodlet *_foodlet;
  Critter *_subject;

  struct InstrumentalizedCritter {
    Critter *critter;
    float pangle = NAN;
    int collisionDelay;
    InstrumentalizedCritter (Critter *c)
      : critter(c), pangle(NAN), collisionDelay(0) {}
  };
  std::vector<InstrumentalizedCritter> _others;

  struct MotorOutput { float l, r; };

  friend MotorOutput& max (MotorOutput &lhs, const MotorOutput &rhs) {
    lhs.l = std::max(lhs.l, rhs.l);
    lhs.r = std::max(lhs.r, rhs.r);
    return lhs;
  }

  void motors(Critter *c, const MotorOutput &mo) {
    c->setMotorOutput(mo.l, Motor::LEFT);
    c->setMotorOutput(mo.r, Motor::RIGHT);
  }

  void motors(Critter *c, float l, float r) {
    motors(c, {l, r});
  }

  bool isSubjectFeeding (void) const;
  bool predatorWon (void) const;

  static const Genome& predatorGenome (void);

  static genotype::Environment environmentGenome (Specs::Type t);
};

} // end of namespace simu

#endif // SCENARIO_H
