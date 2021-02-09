#ifndef GNTP_ENVIRONMENT_H
#define GNTP_ENVIRONMENT_H

#include "kgd/genotype/selfawaregenome.hpp"

namespace genotype {

class Environment : public EDNA<Environment> {
  APT_EDNA()
public:
  int size;
  int taurus;

  float maxVegetalPortion;
};

DECLARE_GENOME_FIELD(Environment, int, size)
DECLARE_GENOME_FIELD(Environment, int, taurus)
DECLARE_GENOME_FIELD(Environment, float, maxVegetalPortion)

} // end of namespace genotype

namespace config {

template <>
struct EDNA_CONFIG_FILE(Environment) {
  DECLARE_PARAMETER(Bounds<int>, sizeBounds)
  DECLARE_PARAMETER(Bounds<int>, taurusBounds)
  DECLARE_PARAMETER(Bounds<float>, maxVegetalPortionBounds)

  DECLARE_PARAMETER(MutationRates, mutationRates)
  DECLARE_PARAMETER(DistanceWeights, distanceWeights)
};

} // end of namespace config

#endif // GNTP_ENVIRONMENT_H
