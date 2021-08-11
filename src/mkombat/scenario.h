#ifndef SCENARIO_H
#define SCENARIO_H

#include "../simu/simulation.h"

namespace simu {

struct Team {
  using Genome = Simulation::CGenome;
  std::vector<Genome> members;
//  std::vector<float> ifitnesses;  /// TODO Implement?
  float fitness;

  Team (void) : fitness(NAN) {}

  auto size (void) const {
    return members.size();
  }

  friend void to_json (nlohmann::json &j, const Team &t);
  friend void from_json (const nlohmann::json &j, Team &t);
  static Team fromFile(const stdfs::path &p);
};

class Scenario {
public:
  Scenario(Simulation &simulation, uint tSize);

  void init (const Team &lhs, const Team &rhs);
  void postEnvStep (void);
  void postStep (void);

//  void applyLesions (int lesions);

  std::array<float,2> scores (void) const;
  std::array<bool,2> brainless (void) const;

  static const Simulation::InitData commonInitData;

private:
  Simulation &_simulation;
  const uint _teamsSize;

  std::array<std::vector<simu::Critter*>, 2> _teams;

  static genotype::Environment environmentGenome (uint tSize);
};

} // end of namespace simu

#endif // SCENARIO_H
