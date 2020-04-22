#ifndef HYPERNEAT_GENOTYPE_WRAPPER_H
#define HYPERNEAT_GENOTYPE_WRAPPER_H

#include "kgd/genotype/selfawaregenome.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#if __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif
#include "../../MultiNEAT/src/Genome.h" // Bad header
#pragma GCC diagnostic pop

namespace genotype {

class HyperNEAT : public genotype::EDNA<HyperNEAT> {
  APT_EDNA()
public:
  NEAT::Genome data;

  void toDot (std::ostream &os) const;
  static void phenotypeToDat (std::ostream &os, const NEAT::NeuralNetwork &n);

  void BuildHyperNEATPhenotype(NEAT::NeuralNetwork &net, NEAT::Substrate &subst) {
    data.BuildHyperNEATPhenotype(net, subst);
  }
};

DECLARE_GENOME_FIELD(HyperNEAT, NEAT::Genome, data)

} // end of namespace genotype

namespace config {

template <>
struct EDNA_CONFIG_FILE(HyperNEAT) {
  static constexpr uint NODE_COORDINATES_DIMENSIONS = 3;  // in R^3

  DECLARE_PARAMETER(bool, splitLoopedRecurrent)
  DECLARE_PARAMETER(bool, splitRecurrent)

  DECLARE_PARAMETER(double, activationAMutationMaxPower)
  DECLARE_PARAMETER(double, activationBMutationMaxPower)
  DECLARE_PARAMETER(double, activationFunction_Abs)
  DECLARE_PARAMETER(double, activationFunction_Linear)
  DECLARE_PARAMETER(double, activationFunction_Relu)
  DECLARE_PARAMETER(double, activationFunction_SignedGauss)
  DECLARE_PARAMETER(double, activationFunction_SignedSigmoid)
  DECLARE_PARAMETER(double, activationFunction_SignedSine)
  DECLARE_PARAMETER(double, activationFunction_SignedStep)
  DECLARE_PARAMETER(double, activationFunction_Softplus)
  DECLARE_PARAMETER(double, activationFunction_Tanh)
  DECLARE_PARAMETER(double, activationFunction_TanhCubic)
  DECLARE_PARAMETER(double, activationFunction_UnsignedGauss)
  DECLARE_PARAMETER(double, activationFunction_UnsignedSigmoid)
  DECLARE_PARAMETER(double, activationFunction_UnsignedSine)
  DECLARE_PARAMETER(double, activationFunction_UnsignedStep)
  DECLARE_PARAMETER(double, biasMutationMaxPower)
  DECLARE_PARAMETER(double, maxActivationA)
  DECLARE_PARAMETER(double, maxActivationB)
  DECLARE_PARAMETER(double, maxWeight)
  DECLARE_PARAMETER(double, minActivationA)
  DECLARE_PARAMETER(double, minActivationB)
  DECLARE_PARAMETER(double, minWeight)
  DECLARE_PARAMETER(double, mutateActivationA)
  DECLARE_PARAMETER(double, mutateActivationB)
  DECLARE_PARAMETER(double, mutateAddLink)
  DECLARE_PARAMETER(double, mutateAddLinkFromBias)
  DECLARE_PARAMETER(double, mutateAddNeuron)
  DECLARE_PARAMETER(double, mutateNeuronActivationType)
  DECLARE_PARAMETER(double, mutateRemLink)
  DECLARE_PARAMETER(double, mutateRemSimpleNeuron)
  DECLARE_PARAMETER(double, mutateWeights)
  DECLARE_PARAMETER(double, mutateWeightsSevere)
  DECLARE_PARAMETER(double, recurrent)
  DECLARE_PARAMETER(double, recurrentLoop)
  DECLARE_PARAMETER(double, timeConstantMutationMaxPower)
  DECLARE_PARAMETER(double, weightMutationMaxPower)
  DECLARE_PARAMETER(double, weightMutationRate)
  DECLARE_PARAMETER(double, weightReplacementMaxPower)
  DECLARE_PARAMETER(double, weightReplacementRate)

  /// Addition. Ratio of min/max weight affected at random creation
  DECLARE_PARAMETER(double, weightInitialRange)

  DECLARE_PARAMETER(int, maxLinks)
  DECLARE_PARAMETER(int, maxNeurons)
  DECLARE_PARAMETER(int, neuronTries)
  DECLARE_PARAMETER(int, linkTries)

  DECLARE_PARAMETER(MutationRates, mutationRates)
  DECLARE_PARAMETER(DistanceWeights, distanceWeights)

  static const NEAT::Parameters& params (void);

  static NEAT::RNG& RNG (void);

  static NEAT::InnovationDatabase& innovations (void);
};

} // end of namespace config

#endif // HYPERNEAT_GENOTYPE_WRAPPER_H
