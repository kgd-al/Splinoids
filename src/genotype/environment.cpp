#include "environment.h"

using namespace genotype;

#define GENOME Environment

DEFINE_GENOME_FIELD_WITH_BOUNDS(int, size, "", 100, 100)
DEFINE_GENOME_FIELD_WITH_BOUNDS(int, taurus, "", 0, 1)

DEFINE_GENOME_MUTATION_RATES({
  EDNA_PAIR(  size, 1.f),
  EDNA_PAIR(taurus, 1.f),
})

DEFINE_GENOME_DISTANCE_WEIGHTS({
  EDNA_PAIR(  size, 1.f),
  EDNA_PAIR(taurus, 1.f),
})
