#ifndef SCENARIO_H
#define SCENARIO_H

#include "../../simu/simulation.h"

DEFINE_NAMESPACE_SCOPED_PRETTY_ENUMERATION(
  eval, Type,
  ERROR=0,
  DIRECTION = 1,
  COLOR = 2)

namespace simu {

class Scenario {
public:
  using Genome = Simulation::CGenome;
  static constexpr uint DURATION = 10; //seconds

  using Type = eval::Type;
  struct Spec {
    uint target; // [0,2]
    std::vector<Color> colors; // last is always right-hand side
  };

  struct Params {
    const phenotype::ANN *brainTemplate;
    Genome genome;
    Type type;
    Spec spec;

    enum Flag {
      NONE
    };
    using Flags = std::bitset<1>;
    Flags flags;
  };

  Scenario(Simulation &simulation);

  void init (const Params &params);
  void postEnvStep (void);
  void postStep (void);
  void preDelCritter (Critter *c);

//  void applyLesions (int lesions);
  void muteReceiver (void);

  const auto& simulation (void) const {
    return _simulation;
  }

  float score (void) const;
  static float minScore (void);
  static float maxScore (void);

  bool mute (void) const {
    return _mute;
  }

  bool neuralEvaluation (void) const {
    return _params.flags.any();
  }

  bool hasFlag (Params::Flag f) {
    return _params.flags.test(f);
  }

  const auto& currentFlags (void) const {
    return _currentFlags;
  }

  const Critter* emitter (void) const {   return _critters[0];  }
  Critter* emitter (void) {
    return const_cast<Critter*>(const_cast<const Scenario*>(this)->emitter());
  }

  const Critter* receiver (void) const {  return _critters[1];  }
  Critter* receiver (void) {
    return const_cast<Critter*>(const_cast<const Scenario*>(this)->receiver());
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
  std::vector<simu::Foodlet*> _foodlets;

  Params _params;
  Params::Flags _currentFlags;

  bool _mute;
  uint _testChannel;

  Critter* makeCritter (uint id, const genotype::Critter &genome,
                        const phenotype::ANN *brainTemplate);
  Foodlet* makeFoodlet (float x, int side, Color c = {0,1,0});

  static genotype::Environment environmentGenome (bool eval);
};

} // end of namespace simu

#endif // SCENARIO_H
