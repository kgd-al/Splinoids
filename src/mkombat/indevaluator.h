#ifndef INDEVALUATOR_H
#define INDEVALUATOR_H

#include "kgd/external/json.hpp"
#include "kgd/external/gaga.hpp"
#include "kgd/external/novelty.hpp"
#include "scenario.h"

namespace simu {

struct Evaluator {
  using Genome = genotype::Critter;

  Evaluator (void);

//  void setLesionTypes (const std::string &s);

//  static void applyNeuralFlags (phenotype::ANN &ann,
//                                const std::string &tagsfile);

  void operator() (Team &lhs, Team &rhs);

  static std::string kombatName (const std::string &lhsFile,
                                 const std::string &rhsFile);

  stdfs::path logsSavePrefix, annTagsFile;

//  std::vector<int> lesions;
  static constexpr std::array<int,1> lesions {0};

  static std::atomic<bool> aborted;
};

} // end of namespace simu

#endif // INDEVALUATOR_H
