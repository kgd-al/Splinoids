#ifndef SCENARIO_H
#define SCENARIO_H

#include "../simu/simulation.h"

namespace simu {

class Scenario {
  using Genome = Simulation::CGenome;
public:  
  struct Specs {
    enum Type { ALONE, CLONE, PREDATOR };
    Type type;
    P2D food, other;

    static Specs fromValues (Type t, int fx, int fy, int ox = 0, int oy = 0);
    static Specs fromString (const std::string &s);
    static std::string toString (const Specs &s);

    static float difficulty (const Specs &s);
  };

  Scenario(const Specs &specs, Simulation &simulation);
  Scenario(const std::string &specs, Simulation &simulation);

  void init (const Genome &genome);
  void postStep (void);

  const auto& subjectBrain (void) const {
    return _subject->brain();
  }

  float score (void) const;

  static const Simulation::InitData commonInitData;

private:
  const Specs _specs;
  Simulation &_simulation;

  Critter *_subject, *_other;
  float _pangle = NAN;
  int _collisionDelay;

  struct MotorOutput { float l, r; };

  friend MotorOutput& max (MotorOutput &lhs, const MotorOutput &rhs) {
    lhs.l = std::max(lhs.l, rhs.l);
    lhs.r = std::max(lhs.r, rhs.r);
    return lhs;
  }

  void motors(const MotorOutput &mo) {
    _other->setMotorOutput(mo.l, Motor::LEFT);
    _other->setMotorOutput(mo.r, Motor::RIGHT);
  }

  void motors(float l, float r) {
    motors({l, r});
  }

  bool isSubjectFeeding (void) const;

  static Genome brainless (Genome g);
  static const Genome& predator (void);
};

} // end of namespace simu

#endif // SCENARIO_H
