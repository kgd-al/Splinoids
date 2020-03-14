#ifndef SIMU_ENVIRONMENT_H
#define SIMU_ENVIRONMENT_H

#include "box2d/box2d.h"

#include "../genotype/environment.h"

namespace simu {

struct Critter;

class Environment {
public:
  using Genome = genotype::Environment;
  using Color = std::array<float, 3>;
  static constexpr Color EMPTY { 0, 0, 0 };

  // Box2D parameters
  static constexpr float DT = 1.f / 60.f;
  static constexpr int V_ITER = 8;  // Solver iterations for velocities
  static constexpr int P_ITER = 3;  // Solver iterations for positions

private:
  Genome _genome;
  b2World _physics;

  b2Body *_edges;

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

  const auto& physics (void) const {
    return _physics;
  }

  auto& physics (void) {
    return _physics;
  }

  b2Body* body (void) {
    return _edges;
  }

  void vision (const Critter *c) const;

  virtual void step (void);

private:
  void createEdges (void);
};

} // end of namespace simu

#endif // SIMU_ENVIRONMENT_H
