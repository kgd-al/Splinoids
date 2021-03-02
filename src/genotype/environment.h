#ifndef GNTP_ENVIRONMENT_H
#define GNTP_ENVIRONMENT_H

#include "kgd/genotype/selfawaregenome.hpp"

namespace genotype {

class Environment : public EDNA<Environment> {
  APT_EDNA()
public:
  int width, height;
  int taurus;

  float maxVegetalPortion;
};

DECLARE_GENOME_FIELD(Environment, int, width)
DECLARE_GENOME_FIELD(Environment, int, height)
DECLARE_GENOME_FIELD(Environment, int, taurus)
DECLARE_GENOME_FIELD(Environment, float, maxVegetalPortion)

} // end of namespace genotype

namespace config {

template <>
struct EDNA_CONFIG_FILE(Environment) {
  DECLARE_PARAMETER(Bounds<int>, widthBounds)
  DECLARE_PARAMETER(Bounds<int>, heightBounds)
  DECLARE_PARAMETER(Bounds<int>, taurusBounds)
  DECLARE_PARAMETER(Bounds<float>, maxVegetalPortionBounds)

  DECLARE_PARAMETER(MutationRates, mutationRates)
  DECLARE_PARAMETER(DistanceWeights, distanceWeights)
};

} // end of namespace config

#endif // GNTP_ENVIRONMENT_H
