#ifndef SCENARIO_H
#define SCENARIO_H

#include "../../simu/simulation.h"

DEFINE_NAMESPACE_SCOPED_PRETTY_ENUMERATION(
  eval, Type,
  ERROR=0,
  NEURAL_EVALUATION = 1,
  DIRECTION = 2,
  COLOR = 4)

namespace simu {

class Scenario {
public:
  using Genome = Simulation::CGenome;
  static constexpr uint DURATION = 10; // seconds

  static constexpr uint EVAL_STEPS = 6; // seconds
  static constexpr uint EVAL_STEP_DURATION = 2; // seconds
  static constexpr uint EVAL_DURATION = EVAL_STEPS * EVAL_STEP_DURATION;

  using Type = eval::Type;
  struct Spec {
    uint target; // [0,2]

    // last is right-hand side (except for neural evaluations)
    std::vector<Color> colors;

    // optional argument (e.g. file name for auditive neural evaluation)
    std::string arg;

    std::string name;

    friend std::ostream& operator<< (std::ostream &os, const Spec &s);
  };

  struct Params {
    const phenotype::ANN *brainTemplate;
    Genome genome;
    Type type;
    Spec spec;

    enum Flag {
      // For Directions:
      //  - vision: 2 (one per role)
      //  - audition: 3 (one per direction)

      // For Colors:
      //  - vision: 2 (one per role, merge all colors)
      //  - audition: n (one per vocalized color, implies some overlap)

      VISION_EMITTER, VISION_RECEIVER,
      AUDITION_0, AUDITION_1, AUDITION_2
    };
    using Flags = std::bitset<5>;
    Flags flags;

    static bool neuralEvaluation(Type t) {
      return uint(t) & uint(Type::NEURAL_EVALUATION);
    }

    bool neuralEvaluation(void) const {
      return neuralEvaluation(type);
    }
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
    return _params.neuralEvaluation();
  }

  bool hasFlag (Params::Flag f) {
    return _params.flags.test(f);
  }

  bool hasVisionFlag (void) {
    return hasFlag(Params::VISION_EMITTER)
        || hasFlag(Params::VISION_RECEIVER);
  }

  bool hasAuditionFlag (void) {
    return hasFlag(Params::AUDITION_0)
        || hasFlag(Params::AUDITION_1)
        || hasFlag(Params::AUDITION_2);
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

  Critter* subject (void) {               return _critters[0];  }

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

  // For neural evaluation. Recording of emitter in 'normal' conditions
  std::vector<float> _injectionData;

  Critter* makeCritter (uint id, const genotype::Critter &genome,
                        const phenotype::ANN *brainTemplate);
  Foodlet* makeFoodlet (float x, int side, Color c = {0,1,0});

  static genotype::Environment environmentGenome (bool eval);
};

} // end of namespace simu

#endif // SCENARIO_H
