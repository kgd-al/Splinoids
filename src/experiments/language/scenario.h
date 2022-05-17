#ifndef SCENARIO_H
#define SCENARIO_H

#include "../../simu/simulation.h"

namespace simu {

class Scenario {
public:
  using Genome = Simulation::CGenome;
  static constexpr uint DURATION = 10; //seconds
  struct Spec {
    enum Type { ERROR=0, DIRECTION, COLOR } type;
    uint target; // [0,2]
    std::array<Color,3> colors;

    static Spec fromString(const std::string &s);
  };

  struct Params {
    Genome genome;
    Spec spec;

    enum Flag {
      NONE
    };
    using Flags = std::bitset<9>;
    Flags flags;
  };


  Scenario(Simulation &simulation);

  void init (const Params &params);
  void postEnvStep (void);
  void postStep (void);
  void preDelCritter (Critter *c);

//  void applyLesions (int lesions);

  const auto& simulation (void) const {
    return _simulation;
  }

  float score (void) const;

  bool neuralEvaluation (void) const {
    return _params.flags.any();
  }

  bool hasFlag (Params::Flag f) {
    return _params.flags.test(f);
  }

  const auto& currentFlags (void) const {
    return _currentFlags;
  }

  Critter* emitter (void) {
    return _critters[0];
  }

  Critter* receiver (void) {
    return _critters[1];
  }

  const auto& critters (void) const {
    return _critters;
  }

  static char critterRole (uint id);
  static char critterRole (phylogeny::GID gid) {
    return critterRole(uint(gid)-1);
  }

  static const Simulation::InitData commonInitData;

private:
  Simulation &_simulation;

  std::vector<simu::Critter*> _critters;

  Params _params;
  Params::Flags _currentFlags;

  uint _testChannel;

  Critter* makeCritter (uint id, const genotype::Critter &genome);

  static genotype::Environment environmentGenome (bool eval);
};

} // end of namespace simu

#endif // SCENARIO_H
