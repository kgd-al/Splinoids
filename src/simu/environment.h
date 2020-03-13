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

private:
  Genome _genome;
  b2World *_physics;

public:
  Environment(const Genome &g);
  ~Environment (void);

  const auto& genotype (void) const {
    return _genome;
  }

  auto size (void) const {
    return _genome.size;
  }

  const auto* physics (void) const {
    return _physics;
  }

  auto* physics (void) {
    return _physics;
  }

  void vision (const Critter *c) const;

private:
};

} // end of namespace simu

#endif // SIMU_ENVIRONMENT_H
