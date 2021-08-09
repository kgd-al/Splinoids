#ifndef SCENARIO_H
#define SCENARIO_H

#include "../simu/simulation.h"

namespace simu {

struct Team {
  using Genome = Simulation::CGenome;
  std::vector<Genome> members;
  std::vector<float> ifitnesses;
  float fitness;

  void to_json (nlohmann::json &j, const Team &t);
  void from_json (const nlohmann::json &j, Team &t);
};

class Scenario {
public:
  Scenario(Simulation &simulation, uint tSize);

  void init (const Team &lhs, const Team &rhs);
  void postEnvStep (void);
  void postStep (void);

//  void applyLesions (int lesions);

  float score (void) const;

  static const Simulation::InitData commonInitData;

private:
  const uint _teamsSize;
  Simulation &_simulation;

  static genotype::Environment environmentGenome (uint tSize);
};

} // end of namespace simu

#endif // SCENARIO_H
