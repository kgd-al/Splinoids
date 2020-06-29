#ifndef HYPERNEAT_PHENOTYPE_WRAPPER_H
#define HYPERNEAT_PHENOTYPE_WRAPPER_H

#include "genotype.h"

#include "../simu/config.h"

namespace simu {

NEAT::Substrate substrateFor (const std::vector<simu::P2D> &rays,
                              uint hiddenLayers, uint visionNeurons);

NEAT::Substrate substrateFor (const std::vector<simu::P2D> &rays,
                              const genotype::HyperNEAT &genome);

} // end of namespace simu

// For ADL
namespace NEAT {

void assertEqual (const Connection &lhs, const Connection &rhs, bool deepcopy);

void assertEqual (const Neuron &lhs, const Neuron &rhs, bool deepcopy);

void assertEqual (const NeuralNetwork &lhs, const NeuralNetwork &rhs,
                  bool deepcopy);

} // end of namespace NEAT

#endif // HYPERNEAT_PHENOTYPE_WRAPPER_H
