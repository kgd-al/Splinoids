#ifndef HYPERNEAT_PHENOTYPE_WRAPPER_H
#define HYPERNEAT_PHENOTYPE_WRAPPER_H

#include "genotype.h"

#include "../simu/config.h"

namespace simu {

NEAT::Substrate substrateFor (const std::vector<simu::P2D> &rays);

} // end of namespace simu

#endif // HYPERNEAT_PHENOTYPE_WRAPPER_H
