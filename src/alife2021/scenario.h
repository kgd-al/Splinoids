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

    static Specs fromString (const std::string &s);
  };

  Scenario(const Specs &specs, Simulation &simulation);
  Scenario(const std::string &specs, Simulation &simulation);

  void init (Simulation &s, const Genome &genome);
  void postStep (Simulation &s);

  static const Simulation::InitData commonInitData;

private:
  const Specs _specs;
  static const Genome& predator (void);
};

} // end of namespace simu

#endif // SCENARIO_H
