#ifndef SCENARIO_H
#define SCENARIO_H

#include "../simu/simulation.h"

namespace simu {

class Scenario {
  using Genome = Simulation::CGenome;
public:  
  struct Specs {
    enum Type : uint {
      ERROR = 0,

      V0_ALONE = 1, V0_CLONE = 2, V0_PREDATOR = 3,
      V1_ALONE = 4, V1_CLONE = 5, V1_PREDATOR = 6, V1_BOTH = 7,

      EVAL = 1<<3,
      EVAL_PREDATOR = 1<<4,
      EVAL_CLONE = 1<<5,
      EVAL_FOOD = 1<<6,

      EVAL_CLONE_INVISIBLE = 1<<7,
      EVAL_CLONE_MUTE = 1<<8
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

  void init (Genome genome);
  void postEnvStep (void);
  void postStep (void);

  void applyLesions (int lesions);

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
    if (_specs.type & (Specs::EVAL | Specs::EVAL_CLONE))
      if (!_others.empty())
        return _others[0].critter;

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
    if (_specs.type & (Specs::EVAL | Specs::EVAL_PREDATOR))
      if (!_others.empty())
        return _others[0].critter;

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

  Critter* predator (void) {
    return const_cast<Critter*>(const_cast<const Scenario*>(this)->predator());
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

  static Genome cloneGenome (Genome g);
  static const Genome& predatorGenome (void);

  static genotype::Environment environmentGenome (Specs::Type t);
};

} // end of namespace simu

#endif // SCENARIO_H
