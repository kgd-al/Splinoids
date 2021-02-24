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
  std::array<std::vector<Specs>, S+1> allScenarios;
  std::vector<Specs> currentScenarios;

  IndEvaluator (bool v0Scenarios);

  void selectCurrentScenarios (rng::AbstractDice &dice);
  void setScenarios (const std::string &s);

  void operator() (Ind &ind, int);

  static Ind fromJsonFile (const std::string &path);

  const bool usingV0Scenarios;
  stdfs::path logsSavePrefix;

  static std::atomic<bool> aborted;
};

} // end of namespace simu

#endif // INDEVALUATOR_H
