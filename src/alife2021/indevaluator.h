#ifndef INDEVALUATOR_H
#define INDEVALUATOR_H

#include "kgd/external/json.hpp"
#include "kgd/external/gaga.hpp"
#include "scenario.h"

namespace simu {

struct IndEvaluator {
  using Specs = Scenario::Specs;
  using DNA = genotype::Critter;
  using GA = GAGA::GA<DNA>;
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
  void setScenarios (const std::string &s);

  void operator() (Ind &ind, int);

  static Ind fromJsonFile (const std::string &path);

  static std::atomic<bool> aborted;
};

} // end of namespace simu

#endif // INDEVALUATOR_H
