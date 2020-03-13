#ifndef SIMU_ENVIRONMENT_H
#define SIMU_ENVIRONMENT_H

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

public:
  Environment(const Genome &g);

  const auto& genotype (void) const {
    return _genome;
  }

  auto size (void) const {
    return _genome.size;
  }

  void vision (const Critter *c);

private:
};

} // end of namespace simu

#endif // SIMU_ENVIRONMENT_H
