#ifndef INDEVALUATOR_H
#define INDEVALUATOR_H

#include "kgd/external/json.hpp"
#include "kgd/external/gaga.hpp"
#include "kgd/external/novelty.hpp"
#include "scenario.h"

namespace simu {

struct IndEvaluator {
  using Specs = Scenario::Specs;
  using DNA = genotype::Critter;
  using Footprint = std::array<float, 16>;
  using Ind = GAGA::NoveltyIndividual<DNA, Footprint>;
  using GA = GAGA::GA<DNA, Ind>;

  static constexpr int S = 3;
  std::array<std::vector<Specs>, S+1> allScenarios;
  std::vector<Specs> currentScenarios;

  IndEvaluator (bool v0Scenarios);

  void selectCurrentScenarios (rng::AbstractDice &dice);
  void setScenarios (const std::string &s);

  void setLesionTypes (const std::string &s);

  static void applyNeuralFlags (phenotype::ANN &ann,
                                const std::string &tagsfile);

  void operator() (Ind &ind, int);

  static Ind fromJsonFile (const std::string &path);

  static std::string specToString (const Specs s, int lesion);

  const bool usingV0Scenarios;
  stdfs::path logsSavePrefix, annTagsFile;

  std::vector<int> lesions;

  static std::atomic<bool> aborted;
};

} // end of namespace simu

#endif // INDEVALUATOR_H
