#ifndef SCENARIO_H
#define SCENARIO_H

#include "../simu/simulation.h"

namespace simu {

struct Team {
  using Genome = Simulation::CGenome;
  std::vector<Genome> members;

  Team (void) {}

  static Team random (uint n, rng::AbstractDice &d) {
    Team t;
    for (uint i=0; i<n; i++)
      t.members.push_back(Genome::random(d));
    return t;
  }

  auto size (void) const {
    return members.size();
  }

  // == Gaga methods
  std::string serialize (void) const {
    return nlohmann::json(*this).dump();
  }

  Team (const std::string &json) {
    *this = nlohmann::json::parse(json);
  }

  friend void to_json (nlohmann::json &j, const Team &t);
  friend void from_json (const nlohmann::json &j, Team &t);

  static Team fromFile(const stdfs::path &p);
  void toFile (const stdfs::path &p);
};

class Scenario {
public:
  static constexpr uint DURATION = 20; //seconds

  Scenario(Simulation &simulation, uint tSize);

  void init (const Team &lhs, const Team &rhs);
  void postEnvStep (void);
  void postStep (void);
  void preDelCritter (Critter *c);

//  void applyLesions (int lesions);

  float score (void) const;
  std::array<bool,2> brainless (void) const;

  const auto& teams (void) const {
    return _teams;
  }

  static const Simulation::InitData commonInitData;

private:
  Simulation &_simulation;
  const uint _teamsSize;

  std::array<std::set<simu::Critter*>, 2> _teams;

  static genotype::Environment environmentGenome (uint tSize);
};

} // end of namespace simu

#endif // SCENARIO_H
