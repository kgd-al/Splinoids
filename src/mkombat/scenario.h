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
  struct Params {
    std::string desc;
    std::string kombatName;

    Team rhs;

    bool neuralEvaluation;

    using Flags = std::bitset<3>;
    Flags flags;

    enum Pain { INSTANTANEOUS, ABSOLUTE, OPPONENT };
  };

  Scenario(Simulation &simulation, uint tSize);

  void init (Team &lhs, const Params &params);
  void postEnvStep (void);
  void postStep (void);
  void preDelCritter (Critter *c);

//  void applyLesions (int lesions);

  const auto& simulation (void) const {
    return _simulation;
  }

  float score (void) const;
  std::array<bool,2> brainless (void) const;

  const auto& teams (void) const {
    return _teams;
  }

  bool neuralEvaluation (void) const {
    return _flags.any();
  }

  const auto& currentFlags (void) const {
    return _currentFlags;
  }

  Critter* subject (void) {
    return *_teams[0].begin();
  }

  Critter* opponent (void) {
    return *_teams[1].begin();
  }

  static const Simulation::InitData commonInitData;

private:
  Simulation &_simulation;
  const uint _teamsSize;

  std::array<std::set<simu::Critter*>, 2> _teams;
  Params::Flags _flags, _currentFlags;

  static genotype::Environment environmentGenome (uint tSize);
};

} // end of namespace simu

#endif // SCENARIO_H
