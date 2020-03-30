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

  // Destroyed splines that must be deleted after the physical step
//  PendingDeletions _pendingDeletions;

  EdgeCritters _edgeCritters;

  decimal _energyReserve;

public:
  Environment(const Genome &g);
  ~Environment (void);

  const auto& genotype (void) const {
    return _genome;
  }

  auto size (void) const {
    return _genome.size;
  }

  auto extent (void) const {
    return .5 * size();
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

  void vision (const Critter *c) const;

  virtual void step (void);

  void modifyEnergyReserve (decimal e);

  decimal energy (void) const {
    return _energyReserve;
  }

  static decimal dt(void);

private:
  void createEdges (void);

  void processFight (Critter *cA, Critter *cB,
                     const FightingData &d,
                     std::set<std::pair<Critter*,uint>> &destroyedSplines);
  void maybeTeleport (Critter *c);
};

} // end of namespace simu

#endif // SIMU_ENVIRONMENT_H
