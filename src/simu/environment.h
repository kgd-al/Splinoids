#ifndef SIMU_ENVIRONMENT_H
#define SIMU_ENVIRONMENT_H

#include "box2d/box2d.h"

#include "../genotype/environment.h"
#include "config.h"

namespace simu {

struct Foodlet;
struct Critter;

struct CollisionMonitor;

class Environment {
  friend CollisionMonitor;
public:
  using Genome = genotype::Environment;

  struct FeedingEvent {
    Critter *critter;
    Foodlet *foodlet;

    friend bool operator< (const FeedingEvent &lhs, const FeedingEvent &rhs) {
      if (lhs.critter != rhs.critter) return lhs.critter < rhs.critter;
      return lhs.foodlet < rhs.foodlet;
    }
  };
  using FeedingEvents = std::set<FeedingEvent>;

  using FightingKey = std::pair<Critter*, Critter*>;
  struct FightingData {
    std::array<P2D, 2> velocities;
    std::set<std::pair<b2Fixture*,b2Fixture*>> fixtures;
  };
  using FightingEvents = std::map<FightingKey, FightingData>;

  using HearingEvents = std::set<std::pair<Critter*,Critter*>>;
  using MatingEvents = std::set<std::pair<Critter*,Critter*>>;

//  using PendingDeletions = std::set<std::pair<Critter*, uint>>;

  using EdgeCritters = std::set<Critter*>;

  struct FightingDrawData {
    P2D pA, pB; // Position of bodies
    P2D vA, vB; // Velocity of bodies
    P2D C, C_;  // Combat axis (raw and normalized)
    float VA, VB; // Combat intensity
  };
#ifndef NDEBUG
  std::vector<FightingDrawData> fightingDrawData;
#endif

  static const bool boxEdges;

private:
  Genome _genome;

  b2World _physics;
  CollisionMonitor *_cmonitor;

  b2Body *_edges;
  b2BodyUserData _edgesUserData;

  FeedingEvents _feedingEvents;
  FightingEvents _fightingEvents;

  HearingEvents _hearingEvents;
  MatingEvents _matingEvents;

  // Destroyed splines that must be deleted after the physical step
//  PendingDeletions _pendingDeletions;

  EdgeCritters _edgeCritters;

  decimal _energyReserve;

  rng::FastDice _dice;

public:
  Environment(const Genome &g);
  ~Environment (void);

  void init (decimal energy, uint rngSeed);

  const auto& genotype (void) const {
    return _genome;
  }

  auto size (void) const {
    return _genome.size;
  }

  auto extent (void) const {
    return .5f * size();
  }

  bool isTaurus (void) const {
    return _genome.taurus;
  }

  const auto& physics (void) const {
    return _physics;
  }

  auto& physics (void) {
    return _physics;
  }

  b2Body* body (void) {
    return _edges;
  }

  const auto& feedingEvents (void) const {
    return _feedingEvents;
  }

  const auto& fightingEvents (void) const {
    return _fightingEvents;
  }

  auto& matingEvents (void) {
    return _matingEvents;
  }

  const auto& matingEvents (void) const {
    return _matingEvents;
  }

  const auto& hearingEvents (void) const {
    return _hearingEvents;
  }

  void vision (const Critter *c) const;

  virtual void step (void);

  void modifyEnergyReserve (decimal e);

  decimal energy (void) const {
    return _energyReserve;
  }

  auto& dice (void) {
    return _dice;
  }

  void mutateController (rng::AbstractDice &dice, float r);

  static decimal dt(void);

  static Environment* clone (const Environment &e);
  friend void assertEqual (const Environment &lhs, const Environment &rhs,
                           bool deepcopy);

  static void save (nlohmann::json &j, const Environment &e);
  static void load (const nlohmann::json &j, std::unique_ptr<Environment> &e);

private:
  void createEdges (void);

  void processFight (Critter *cA, Critter *cB,
                     const FightingData &d,
                     std::set<std::pair<Critter*,uint>> &destroyedSplines);
  void maybeTeleport (Critter *c);
};

} // end of namespace simu

#endif // SIMU_ENVIRONMENT_H
