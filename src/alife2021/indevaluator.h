#ifndef INDEVALUATOR_H
#define INDEVALUATOR_H

#include "scenario.h"
#include "../../gaga/gaga.hpp"

namespace simu {

struct IndEvaluator {
  using Specs = Scenario::Specs;
  using GA = GAGA::GA<genotype::Critter>;
  using Ind = GA::Ind_t;

  static constexpr int S = 3;
  static constexpr std::array<const char*, S> scenarioLabels {{
    "T0", "T1", "T2"
  }};
  std::array<std::vector<Specs>, S> allScenarios;
  std::array<Specs, S> currentScenarios;

  const genotype::Environment egenome;

  IndEvaluator (const genotype::Environment &e);

  void selectCurrentScenarios (rng::AbstractDice &dice);

  void operator() (Ind &ind, int);
};

} // end of namespace simu

#endif // INDEVALUATOR_H
