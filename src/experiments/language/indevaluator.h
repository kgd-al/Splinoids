#ifndef INDEVALUATOR_H
#define INDEVALUATOR_H

#include "kgd/external/json.hpp"
#include "kgd/external/gaga.hpp"
#include "kgd/external/novelty.hpp"
#include "scenario.h"

namespace simu {

struct Evaluator {
  using Genome = Scenario::Genome;
  struct LogData;

  using Footprint = std::vector<float>;
  static uint footprintSize (uint evaluations);
  static std::vector<std::string> footprintFields (uint evaluations);
  using Spec = Scenario::Spec;
  using Specs = std::vector<Spec>;

  using Ind = GAGA::NoveltyIndividual<Genome, Footprint>;
  using GA = GAGA::GA<Genome, Ind>;

  using Inds = std::vector<Ind>;
  struct Params {
    Ind ind;

    Specs specs;

    Scenario::Params::Flags flags;

    Params (Ind i) : ind(i) {}

    static Params fromArgv (const std::string &indFile,
                            const std::string &scenario);

    Scenario::Params scenarioParams (uint i) const;
  };

  Evaluator (void);

//  void setLesionTypes (const std::string &s);

  static void applyNeuralFlags (phenotype::ANN &ann,
                                const std::string &tagsfile);

  // Regular combat between individuals
  void operator() (Ind &ind);

  // Actual evaluator
  void operator() (Params &params);

  LogData* logging_getData (void);
  void logging_init (LogData *d, const stdfs::path &folder, Scenario &s);
  void logging_step (LogData *d, Scenario &s);
  void logging_freeData (LogData **d);

  static std::string id (const Ind &i) {
    return GAGA::concat(i.id.first, ":", i.id.second, ":", i.dna.id());
  }

  static Ind fromJsonFile (const std::string &path);

  static bool neuralEvaluation (const std::string &scenario);

  static void dumpStats (const stdfs::path &dna, const stdfs::path &folder);

  stdfs::path logsSavePrefix, annTagsFile;

  std::vector<std::unique_ptr<phenotype::ModularANN>> manns;

//  std::vector<int> lesions;
  static constexpr std::array<int,1> lesions {0};

  static std::atomic<bool> aborted;
};

} // end of namespace simu

#endif // INDEVALUATOR_H
