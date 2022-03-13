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

  using Footprint = std::vector<float>;
  static uint footprintSize (uint evaluations);
  static std::vector<std::string> footprintFields (uint evaluations);

  using Ind = GAGA::NoveltyIndividual<Genome, Footprint>;
  using GA = GAGA::GA<Genome, Ind>;

  using Inds = std::vector<Ind>;
  struct Params {
    Ind ind;
    Inds opps;

    std::vector<std::string> kombatNames, oppsId;
    std::string scenario;
    int teamSize;

    Scenario::Params::Flags flags;
    bool neutralFirst;

    Params (Ind i) : ind(i) {}

    static Params fromArgv (const std::string &lhsArg,
                            const std::vector<std::string> &rhsArgs,
                            const std::string &scenario,
                            int teamSize);

    static Params fromInds (Ind &ind, const Inds &opps);

    Scenario::Params scenarioParams (uint i) const;
    std::string opponentsIds (void) const;
  };

  Evaluator (void);

//  void setLesionTypes (const std::string &s);

  static void applyNeuralFlags (phenotype::ANN &ann,
                                const std::string &tagsfile);

  // Regular combat between individuals
  void operator() (Ind &ind, const Inds &opp);

  // Actual evaluator
  void operator() (Params &params);

  static LogData* logging_getData (void);
  static void logging_init (LogData *d, const stdfs::path &folder, Scenario &s);
  static void logging_step (LogData *d, Scenario &s);
  static void logging_freeData (LogData **d);

  static std::string id (const Ind &i) {
    return GAGA::concat(i.id.first, ":", i.id.second,
                        ":", i.dna.genome.id());
  }

  static Ind fromJsonFile (const std::string &path);

  static bool neuralEvaluation (const std::string &scenario);
  static std::string kombatName (const std::string &lhsFile,
                                 const std::string &rhsFile,
                                 const std::string &scenario);

  static void dumpStats (const stdfs::path &dna, const stdfs::path &folder);

  stdfs::path logsSavePrefix, annTagsFile;
  int annAggregation = 0;

//  std::vector<int> lesions;
  static constexpr std::array<int,1> lesions {0};

  static std::atomic<bool> aborted;
};

} // end of namespace simu

#endif // INDEVALUATOR_H
