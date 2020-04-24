#include "genotype.h"
#include "../genotype/critter.h"
#include "phenotype.h"

using namespace genotype;

#define GENOME HyperNEAT
using HN = NEAT::Genome;
using Config = GENOME::config_t;

static constexpr auto ACT_FUNC_HIDDEN = NEAT::TANH;
static constexpr auto ACT_FUNC_OUTPUT = NEAT::TANH;

namespace NEAT {

void assertEqual(const HN &/*lhs*/, const HN &/*rhs*/, bool /*deepcopy*/) {
  assert(false);
}

}

void genotype::HyperNEAT::toDot (std::ostream &os) const {
  static const std::map<NEAT::ActivationFunction, std::string> m {
    {   NEAT::SIGNED_SIGMOID, "sgm"     },
    { NEAT::UNSIGNED_SIGMOID, "|sgm|"   },
    {             NEAT::TANH, "tanh"    },
    {       NEAT::TANH_CUBIC, "tanh3"   },
    {      NEAT::SIGNED_STEP, "step"    },
    {    NEAT::UNSIGNED_STEP, "|step|"  },
    {     NEAT::SIGNED_GAUSS, "gauss"   },
    {   NEAT::UNSIGNED_GAUSS, "|gauss|" },
    {              NEAT::ABS, "abs"     },
    {      NEAT::SIGNED_SINE, "sin"     },
    {    NEAT::UNSIGNED_SINE, "|sin|"   },
    {           NEAT::LINEAR, "x"       },
    {             NEAT::RELU, "relu"    },
    {         NEAT::SOFTPLUS, "soft+"   },
  };

  std::map<NEAT::NeuronType, std::vector<uint>> layers;

  os << "digraph{\n";
  for (const auto &ng: data.m_NeuronGenes) {
    os << "\t" << ng.m_ID << " [label=\"";
    switch (ng.m_Type) {
    case NEAT::NONE:  os << "?"; break;
    case NEAT::INPUT: os << "I"; break;
    case NEAT::BIAS:  os << "Bias"; break;
    case NEAT::HIDDEN:
    case NEAT::OUTPUT:  os << m.at(ng.m_ActFunction); break;
    }

    os << " (" << ng.m_ID << ")\"];\n";

    layers[ng.m_Type].push_back(ng.m_ID);
  }

  os << "\n";
  for (const auto &l: layers) {
    os << "\t{rank=same;";
    for (auto i: l.second) os << " " << i;
    os << "};\n";
  }
  os << "\n";

  for (const auto &lg: data.m_LinkGenes) {
    os << "\t" << lg.m_FromNeuronID << " -> " << lg.m_ToNeuronID
       << " [label=\"" << lg.m_Weight << "\"];\n";
  }
  os << "}\n";
}

struct NS {
  const NEAT::Neuron &n;
  NS (const NEAT::Neuron &n) : n(n) {}
  friend std::ostream& operator<< (std::ostream &os, const NS &ns) {
    const auto &c = ns.n.m_substrate_coords;
    os << c.front();
    for (uint i=1; i<c.size(); i++) os << " " << c[i];
    return os;
  }
};
void genotype::HyperNEAT::phenotypeToDat (std::ostream &os,
                                          const NEAT::NeuralNetwork &n) {

  static const auto W = Config::maxWeight();
  const auto &neurons = n.m_neurons;
  for (const auto &n: neurons)
    os << NS(n) << " " << n.m_type << "\n";

  os << "\n";
  for (const auto &c: n.m_connections) {
    auto cs = neurons[c.m_source_neuron_idx].m_substrate_coords;
    auto cd = neurons[c.m_target_neuron_idx].m_substrate_coords;
    os << NS(neurons[c.m_source_neuron_idx]) << " ";
    for (uint i=0; i<cd.size(); i++)
      os << cd[i] - cs[i] << " ";
    os << c.m_weight / W << "\n";
  }
  os << "\n";
}

auto getNEATDice (rng::AbstractDice &dice) {
  NEAT::RNG rng;
  rng.Seed(dice(std::numeric_limits<long>::min(),
                std::numeric_limits<long>::max()));
  return rng;
}

// Copied (and altered) from MultiNEAT/src/Species.cpp::MutateGenome
void mutateHyperNEATGenome (HN &g, rng::AbstractDice &dice) {
  NEAT::InnovationDatabase &idb = Config::innovations();
  const NEAT::Parameters &params = Config::params();
  auto rng = getNEATDice(dice);

  if (rng.RandFloat() < params.MutateAddNeuronProb) {
    if (params.MaxNeurons > 0) {
      if ((g.NumNeurons() - (g.NumInputs() + g.NumOutputs())) < params.MaxNeurons)
        g.Mutate_AddNeuron(idb, params, rng);

    } else
      g.Mutate_AddNeuron(idb, params, rng);

  } else if (rng.RandFloat() < params.MutateAddLinkProb) {
    if (params.MaxLinks > 0) {
      if (g.NumLinks() < params.MaxLinks)
        g.Mutate_AddLink(idb, params, rng);

    } else
      g.Mutate_AddLink(idb, params, rng);

  } else if (rng.RandFloat() < params.MutateRemSimpleNeuronProb)
    g.Mutate_RemoveSimpleNeuron(idb, params, rng);

  else if (rng.RandFloat() < params.MutateRemLinkProb) {
    // Keep doing this mutation until it is sure that the baby will not
    // end up having dead ends or no links
    HN tmp = g;
    bool no_links = false, has_dead_ends = false;

    int tries = 128;
    do {
      tries--;
      if (tries <= 0) {
          tmp = g;
          break; // give up
      }

      tmp = g;
      tmp.Mutate_RemoveLink(rng);

      no_links = has_dead_ends = false;

      if (tmp.NumLinks() == 0)
          no_links = true;

      has_dead_ends = tmp.HasDeadEnds();
    }
    while (no_links || has_dead_ends);

    g = tmp;

  } else {
    if (rng.RandFloat() < params.MutateNeuronActivationTypeProb)
      g.Mutate_NeuronActivation_Type(params, rng);

    if (rng.RandFloat() < params.MutateWeightsProb)
      g.Mutate_LinkWeights(params, rng);

    if (rng.RandFloat() < params.MutateActivationAProb)
      g.Mutate_NeuronActivations_A(params, rng);

    if (rng.RandFloat() < params.MutateActivationBProb)
      g.Mutate_NeuronActivations_B(params, rng);

    if (rng.RandFloat() < params.MutateNeuronTimeConstantsProb)
      g.Mutate_NeuronTimeConstants(params, rng);

    if (rng.RandFloat() < params.MutateNeuronBiasesProb)
      g.Mutate_NeuronBiases(params, rng);

    if (rng.RandFloat() < params.MutateNeuronTraitsProb)
      g.Mutate_NeuronTraits(params, rng);

    if (rng.RandFloat() < params.MutateLinkTraitsProb)
      g.Mutate_LinkTraits(params, rng);

    if (rng.RandFloat() < params.MutateGenomeTraitsProb)
      g.Mutate_GenomeTraits(params, rng);
  }
}

auto hyperNeatFunctor = [] {
  GENOME_FIELD_FUNCTOR(HN, data) functor;

  functor.random = [] (auto &dice) {
    auto s = simu::substrateFor({});
    HN hn (0, s.GetMinCPPNInputs(), 0, s.GetMinCPPNOutputs(),
           false,
           ACT_FUNC_OUTPUT, ACT_FUNC_HIDDEN,
           0, Config::params(), 0, 0);

    auto rmin = Config::minWeight() * Config::weightInitialRange(),
         rmax = Config::maxWeight() * Config::weightInitialRange();
    for (NEAT::LinkGene &l: hn.m_LinkGenes) l.m_Weight += dice(rmin, rmax);

    return hn;
  };

  functor.mutate = [] (auto &hn, auto &dice) {
    mutateHyperNEATGenome(hn, dice);
  };

  functor.cross = [] (auto &lhs, auto &rhs, auto &dice) {
    // Could just go with a const_cast but I don't trust the underlying code
    HN lhs_copy = lhs, rhs_copy = rhs;
    auto params = Config::params();
    auto rng = getNEATDice(dice);

    HN res = lhs_copy.Mate(rhs_copy, false, true, rng, params);
    return res;
  };

  functor.distance = [] (auto &lhs, auto &rhs) {
    /// ERROR Not implemented
    return 0;
  };

  functor.check = [] (auto &set) {
    /// ERROR Not implemented
    return true;
  };

  return functor;
};

DEFINE_GENOME_FIELD_WITH_FUNCTOR(HN, data, "", hyperNeatFunctor())

DEFINE_GENOME_MUTATION_RATES({ EDNA_PAIR(data, 1) })
DEFINE_GENOME_DISTANCE_WEIGHTS({ EDNA_PAIR(data, 1) })



namespace utils {

std::ostream& operator<< (std::ostream &os, const HN &hn) {
  os << "GenomeStart" << hn.GetID() << "\n";

  // loop over the neurons and save each one
  for (const auto &ng: hn.m_NeuronGenes)  // Save neuron
    os << "Neuron " << ng.ID() << " " << static_cast<int>(ng.Type()) << " "
       << ng.SplitY() << " " << static_cast<int>(ng.m_ActFunction) << " "
       << ng.m_A << " " << ng.m_B << " " << ng.m_TimeConstant << " "
       << ng.m_Bias << "\n";
      // TODO write neuron traits

  // loop over the connections and save each one
  for (const auto &lg: hn.m_LinkGenes)
    os << "Link " << " " << lg.FromNeuronID() << " " << lg.ToNeuronID() << " "
       << lg.InnovationID() << " " << static_cast<int>(lg.IsRecurrent()) << " "
       << lg.GetWeight() << "\n";
      // TODO write link traits

  os << "GenomeEnd\n\n";
  return os;
}

} // end of namespace utils

namespace nlohmann {
template <> struct adl_serializer<HN> {
  static void to_json(json& j, const HN &hn) {
    nlohmann::json jn, jl;
    for (const NEAT::NeuronGene &n: hn.m_NeuronGenes)
      jn.push_back({ n.m_ID, n.m_Type, n.m_SplitY, n.m_ActFunction, n.m_A,
                     n.m_B, n.m_TimeConstant, n.m_Bias });
    for (const NEAT::LinkGene &l: hn.m_LinkGenes)
      jl.push_back({ l.m_FromNeuronID, l.m_ToNeuronID, l.m_InnovationID,
                     l.m_IsRecurrent, l.m_Weight });
    j = { hn.NumInputs(), hn.NumOutputs(), jn, jl };
  }

  static void from_json(const json& j, HN &hn) {
    uint inputs = j[0], outputs = j[1];
    hn = HN(0, inputs, 0, outputs, false, ACT_FUNC_OUTPUT, ACT_FUNC_HIDDEN,
            0, Config::params(), 0, 0);
    hn.m_NeuronGenes.clear();
    hn.m_LinkGenes.clear();

    for (const auto &jn: j[2]) {
      NEAT::NeuronGene n;
      uint i=0;
      n.m_ID = jn[i++];
      n.m_Type = jn[i++];
      n.m_SplitY = jn[i++];
      n.m_ActFunction = jn[i++];
      n.m_A = jn[i++];
      n.m_B = jn[i++];
      n.m_TimeConstant = jn[i++];
      n.m_Bias = jn[i++];
      hn.m_NeuronGenes.push_back(n);
    }

    for (const auto &jl: j[3]) {
      NEAT::LinkGene l;
      uint i=0;
      l.m_FromNeuronID = jl[i++];
      l.m_ToNeuronID = jl[i++];
      l.m_InnovationID = jl[i++];
      l.m_IsRecurrent = jl[i++];
      l.m_Weight = jl[i++];
      hn.m_LinkGenes.push_back(l);
    }
  }
};
}

template <>
struct genotype::MutationRatesPrinter<HN, GENOME, &GENOME::data> {
  static constexpr bool recursive = false;
  static void print (std::ostream &os, uint width, uint depth, float ratio) {
//    prettyPrintMutationRate(os, width, depth, ratio, 1, "nan", true);
  }
};

template <>
struct genotype::Aggregator<HN, HyperNEAT> {
  void operator() (std::ostream &/*os*/, const std::vector<HyperNEAT> &/*genomes*/,
                   std::function<const HN& (const HyperNEAT&)> /*access*/,
                   uint /*verbosity*/) {}
};

template <>
struct genotype::Extractor<HN> {
  std::string operator() (const HN &ls, const std::string &field) {
    return "Not implemented";
  }
};

#undef GENOME

#define CFILE Config

DEFINE_PARAMETER(bool, splitLoopedRecurrent, false)
DEFINE_PARAMETER(bool, splitRecurrent, false)

DEFINE_PARAMETER(double, activationAMutationMaxPower, 0)
DEFINE_PARAMETER(double, activationBMutationMaxPower, 0)

DEFINE_PARAMETER(double, activationFunction_Abs, 0.)
DEFINE_PARAMETER(double, activationFunction_Linear, 1.)
DEFINE_PARAMETER(double, activationFunction_Relu, 0.)
DEFINE_PARAMETER(double, activationFunction_SignedGauss, 1.)
DEFINE_PARAMETER(double, activationFunction_SignedSigmoid, 0.)
DEFINE_PARAMETER(double, activationFunction_SignedSine, 0.)
DEFINE_PARAMETER(double, activationFunction_SignedStep, 1.)
DEFINE_PARAMETER(double, activationFunction_Softplus, 0.)
DEFINE_PARAMETER(double, activationFunction_Tanh, 1.)
DEFINE_PARAMETER(double, activationFunction_TanhCubic, 0.)
DEFINE_PARAMETER(double, activationFunction_UnsignedGauss, 0.)
DEFINE_PARAMETER(double, activationFunction_UnsignedSigmoid, 0.)
DEFINE_PARAMETER(double, activationFunction_UnsignedSine, 0.)
DEFINE_PARAMETER(double, activationFunction_UnsignedStep, 0.)

DEFINE_PARAMETER(double, biasMutationMaxPower, .2)
DEFINE_PARAMETER(double, weightMutationMaxPower, .2)
DEFINE_PARAMETER(double, timeConstantMutationMaxPower, 0.)

DEFINE_PARAMETER(double, minActivationA, .05)
DEFINE_PARAMETER(double, maxActivationA, 6.)
DEFINE_PARAMETER(double, minActivationB, 0)
DEFINE_PARAMETER(double, maxActivationB, 0)

DEFINE_PARAMETER(double, minWeight, -8.)
DEFINE_PARAMETER(double, maxWeight,  8.)

DEFINE_PARAMETER(double, mutateActivationA, 0)
DEFINE_PARAMETER(double, mutateActivationB, 0)

DEFINE_PARAMETER(double, mutateAddLink, .08)
DEFINE_PARAMETER(double, mutateAddLinkFromBias, 0)
DEFINE_PARAMETER(double, mutateAddNeuron, .01)
DEFINE_PARAMETER(double, mutateNeuronActivationType, .03)
DEFINE_PARAMETER(double, mutateRemLink, .02)
DEFINE_PARAMETER(double, mutateRemSimpleNeuron, 0)
DEFINE_PARAMETER(double, mutateWeights, .9)
DEFINE_PARAMETER(double, mutateWeightsSevere, .25)

DEFINE_PARAMETER(double, recurrent, 0)
DEFINE_PARAMETER(double, recurrentLoop, 0)
DEFINE_PARAMETER(double, weightMutationRate, 1)
DEFINE_PARAMETER(double, weightReplacementMaxPower, 1)
DEFINE_PARAMETER(double, weightReplacementRate, .2)


DEFINE_PARAMETER(double, weightInitialRange, .05)


DEFINE_PARAMETER(int, maxLinks, -1)
DEFINE_PARAMETER(int, maxNeurons, -1)
DEFINE_PARAMETER(int, neuronTries, 64)
DEFINE_PARAMETER(int, linkTries, 64)

const NEAT::Parameters& Config::params (void) {
  static const auto p = [] {
    NEAT::Parameters params {};

    params.ActivationAMutationMaxPower = activationAMutationMaxPower();
    params.ActivationBMutationMaxPower = activationBMutationMaxPower();
    params.ActivationFunction_Abs_Prob = activationFunction_Abs();
    params.ActivationFunction_Linear_Prob = activationFunction_Linear();
    params.ActivationFunction_Relu_Prob = activationFunction_Relu();
    params.ActivationFunction_SignedGauss_Prob = activationFunction_SignedGauss();
    params.ActivationFunction_SignedSigmoid_Prob = activationFunction_SignedSigmoid();
    params.ActivationFunction_SignedSine_Prob = activationFunction_SignedSine();
    params.ActivationFunction_SignedStep_Prob = activationFunction_SignedStep();
    params.ActivationFunction_Softplus_Prob = activationFunction_Softplus();
    params.ActivationFunction_TanhCubic_Prob = activationFunction_TanhCubic();
    params.ActivationFunction_Tanh_Prob = activationFunction_Tanh();
    params.ActivationFunction_UnsignedGauss_Prob = activationFunction_UnsignedGauss();
    params.ActivationFunction_UnsignedSigmoid_Prob = activationFunction_UnsignedSigmoid();
    params.ActivationFunction_UnsignedSine_Prob = activationFunction_UnsignedSine();
    params.ActivationFunction_UnsignedStep_Prob = activationFunction_UnsignedStep();
    params.BiasMutationMaxPower = biasMutationMaxPower();
    params.LinkTries = linkTries();
    params.MaxActivationA = maxActivationA();
    params.MaxActivationB = maxActivationB();
    params.MaxLinks = maxLinks();
    params.MaxNeurons = maxNeurons();
    params.MaxWeight = maxWeight();
    params.MinActivationA = minActivationA();
    params.MinActivationB = minActivationB();
    params.MinWeight = minWeight();
    params.MutateActivationAProb = mutateActivationA();
    params.MutateActivationBProb = mutateActivationB();
    params.MutateAddLinkFromBiasProb = mutateAddLinkFromBias();
    params.MutateAddLinkProb = mutateAddLink();
    params.MutateAddNeuronProb = mutateAddNeuron();
    params.MutateNeuronActivationTypeProb = mutateNeuronActivationType();
    params.MutateRemLinkProb = mutateRemLink();
    params.MutateRemSimpleNeuronProb = mutateRemSimpleNeuron();
    params.MutateWeightsProb = mutateWeights();
    params.MutateWeightsSevereProb = mutateWeightsSevere();
    params.NeuronTries = neuronTries();
    params.RecurrentLoopProb = recurrentLoop();
    params.RecurrentProb = recurrent();
    params.SplitLoopedRecurrent = splitLoopedRecurrent();
    params.SplitRecurrent = splitRecurrent();
    params.TimeConstantMutationMaxPower = timeConstantMutationMaxPower();
    params.WeightMutationMaxPower = weightMutationMaxPower();
    params.WeightMutationRate = weightMutationRate();
    params.WeightReplacementMaxPower = weightReplacementMaxPower();
    params.WeightReplacementRate = weightReplacementRate();

    return params;
  }();
  return p;
}

NEAT::InnovationDatabase& Config::innovations (void) {
  static auto idb = [] {
    NEAT::InnovationDatabase IDB;
    IDB.Init(100, 100);
    return IDB;
  }();
  return idb;
}

#undef CFILE
