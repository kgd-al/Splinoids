#ifndef SCENARIO_H
#define SCENARIO_H

#include "../simu/simulation.h"

namespace simu {

struct Team {
  using Genome = Simulation::CGenome;
  Genome genome;
  uint size;

  Team (void) {}

  static Team random (uint n, rng::AbstractDice &d) {
    Team t;
    t.genome = Genome::random(d);
    t.size = n;
    return t;
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
    Team lhs, rhs;

    enum Flag { PAIN_SELF, PAIN_OTHER, PAIN_ALLY };
    using Flags = std::bitset<3>;
    Flags flags;
  };
  using DeathCause = Simulation::Autopsies::Death;
  static constexpr DeathCause UNKNOWN_DEATH_CAUSE = DeathCause(-1);

  Scenario(Simulation &simulation, uint tSize);

  void init (const Params &params);
  void postEnvStep (void);
  void postStep (void);
  void preDelCritter (Critter *c);

//  void applyLesions (int lesions);

  const auto& simulation (void) const {
    return _simulation;
  }

  float score (void) const;
  std::array<bool,2> brainless (void) const;
  const auto& autopsies (void) const {
    return _autopsies;
  }

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

  Critter* ally (void) {
    return *std::next(_teams[0].begin());
  }

  Critter* opponent (void) {
    return *_teams[1].begin();
  }

  static const Simulation::InitData commonInitData;

private:
  Simulation &_simulation;
  uint _teamsSize;

  std::array<std::set<simu::Critter*>, 2> _teams;
  Params::Flags _flags, _currentFlags;

  std::array<std::array<uint, 2>, 2> _autopsies;

  static genotype::Environment environmentGenome (uint tSize);
};

} // end of namespace simu

#endif // SCENARIO_H
