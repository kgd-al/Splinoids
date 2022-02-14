#ifndef INDEVALUATOR_H
#define INDEVALUATOR_H

#include "kgd/external/json.hpp"
#include "kgd/external/gaga.hpp"
#include "kgd/external/novelty.hpp"
#include "scenario.h"

namespace simu {

struct Evaluator {
  using Genome = Team;//genotype::Critter;
  struct LogData;

  static constexpr int FOOTPRINT_DIM =
      4*Critter::SPLINES_COUNT+1  // Start and final healths for splines & body
    + 2                           // Mass and moment of inertia
    + 2;                          // Final position
  using Footprint = std::array<float, FOOTPRINT_DIM>;

  using Ind = GAGA::NoveltyIndividual<Genome, Footprint>;
  using GA = GAGA::GA<Genome, Ind>;

  Evaluator (void);

//  void setLesionTypes (const std::string &s);

//  static void applyNeuralFlags (phenotype::ANN &ann,
//                                const std::string &tagsfile);

  // Regular combat between individuals
  void operator() (Ind &lhs, const Ind &rhs);

  // Actual evaluator
  void operator() (Ind &lhs, const Scenario::Params &params);

  static LogData* logging_getData (void);
  static void logging_init (LogData *d, const stdfs::path &folder, Scenario &s);
  static void logging_step (LogData *d, Scenario &s);
  static void logging_freeData (LogData **d);

  static std::string id (const Ind &i) {
    return GAGA::concat(i.id.first, ":", i.id.second,
                        ":", i.dna.members[0].id());
  }

  static Ind fromJsonFile (const std::string &path);

  static Scenario::Params scenarioFromStrings (const std::string &lhsArg,
                                      const std::string &rhsArg);
  static std::string kombatName (const std::string &lhsFile,
                                 const std::string &rhsArg);

  stdfs::path logsSavePrefix, annTagsFile;

//  std::vector<int> lesions;
  static constexpr std::array<int,1> lesions {0};

  static const std::set<std::string> canonicalScenarios;

  static std::atomic<bool> aborted;
};

} // end of namespace simu

#endif // INDEVALUATOR_H
