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

  using Spec = Scenario::Spec;
  using Specs = std::vector<Spec>;

  struct Params {
    Scenario::Type type;
    Specs specs;

    std::vector<Scenario::Params::Flags> flags;

    static Params fromArgv (const std::string &scenario,
                            const std::string &optarg = "");

    Scenario::Params scenarioParams (uint i) const;

    bool neuralEvaluation (void) const;
    static bool neuralEvaluation (const std::string &scenario);
  };

  using Footprint = std::vector<float>;
  static Footprint footprint (const Params &p);
  static std::vector<std::string> footprintFields (const Params &p);

  using Ind = GAGA::NoveltyIndividual<Genome, Footprint>;
  using GA = GAGA::GA<Genome, Ind>;

  Evaluator (const std::string &scenarioType,
             const std::string &scenarioArg = "");

//  void setLesionTypes (const std::string &s);

  static void applyNeuralFlags (phenotype::ANN &ann,
                                const std::string &tagsfile);

  // Evolver operator
  void operator() (Ind &ind);

  // Actual evaluator
  void operator() (Ind &ind, Params &params);

  LogData* logging_getData (void);
  void logging_init (LogData *d, const stdfs::path &folder, Scenario &s);
  void logging_step (LogData *d, Scenario &s);
  void logging_freeData (LogData **d);

  static std::string id (const Ind &i) {
    return GAGA::concat(i.id.first, ":", i.id.second, ":", i.dna.id());
  }

  static Ind fromJsonFile (const std::string &path);

  static void dumpStats (const stdfs::path &dna, const stdfs::path &folder);

  static std::string prettyEvalTypes (void);

  stdfs::path logsSavePrefix, annTagsFile;

  std::vector<std::unique_ptr<phenotype::ModularANN>> manns;

  static std::atomic<bool> aborted;

  Params params;
  bool muteReceiver = false;
};

} // end of namespace simu

#endif // INDEVALUATOR_H
