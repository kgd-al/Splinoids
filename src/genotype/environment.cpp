#include "environment.h"

using namespace genotype;

#define GENOME Environment

DEFINE_GENOME_FIELD_WITH_BOUNDS(int, width, "", 2, 100, 100, 1000)
DEFINE_GENOME_FIELD_WITH_BOUNDS(int, height, "", 2, 100, 100, 1000)
DEFINE_GENOME_FIELD_WITH_BOUNDS(int, taurus, "", 0, 1)

DEFINE_GENOME_FIELD_WITH_BOUNDS(float, maxVegetalPortion, "mvp",
                                0.f, 1.f, 1.f, 1.f)

DEFINE_GENOME_MUTATION_RATES({
  EDNA_PAIR(            width, 0.f),
  EDNA_PAIR(           height, 0.f),
  EDNA_PAIR(           taurus, 0.f),
  EDNA_PAIR(maxVegetalPortion, 1.f),
})

DEFINE_GENOME_DISTANCE_WEIGHTS({
  EDNA_PAIR(            width, 0.f),
  EDNA_PAIR(           height, 0.f),
  EDNA_PAIR(           taurus, 0.f),
  EDNA_PAIR(maxVegetalPortion, 1.f),
})
