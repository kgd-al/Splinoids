#include "phenotype.h"

namespace simu {

NEAT::Substrate substrateFor (const std::vector<simu::P2D> &rays,
                              const genotype::HyperNEAT &genome) {
  return substrateFor(rays,
                      genome.hiddenNeuronLayers,
                      genome.hiddenNeuronVision);
}

NEAT::Substrate substrateFor (const std::vector<simu::P2D> &rays,
                              uint hiddenLayers, uint visionNeurons) {

  using Coordinates = decltype(NEAT::Substrate::m_input_coords);
  Coordinates inputs, hidden, outputs;

  static const auto add = [] (auto &v, auto x, auto y, auto z) {
    v.emplace_back(Coordinates::value_type({x,y,z}));
  };

  // Inputs
  add(inputs,  .0,   .0,  -.5); // sex
  add(inputs,  .25,  .0,  -.5); // age
  add(inputs, -.25,  .0,  -.5); // reproduction
  add(inputs,  .0,  -.25, -.5); // energy
  add(inputs,  .0,   .25, -.5); // health
  for (const simu::P2D &r: rays) {
    simu::P2D r_ = r;
    r_.Normalize();
    for (uint i=0; i<3; i++) add(inputs, r_.x, r_.y, float(i)-1);

    // old formula for color: -.2*i-.6
    // new formula is just: -1 -> r, 0 -> g, 1 -> b
  }

  // Hidden  
  if (hiddenLayers > 0) {
    static constexpr uint N = 5;
    for (uint i=0; i<N; i++) {
      simu::P2D p = fromPolar(2 * M_PI * i / N, .25);
      add(hidden, p.x, p.y, 0.);

      if (hiddenLayers > 1) {
        for (uint i=0; i<N; i++) {
          simu::P2D p_ = p + fromPolar(2 * M_PI * i / N, .1);
          add(hidden, p_.x, p_.y, 0.);
        }
      }
    }
  }

  // Visual
  if (visionNeurons > 0) {
    float a = 0;
    for (uint i=0; i<rays.size()/2; i++)  a += std::atan2(rays[i].y, rays[i].x);
    a /= rays.size() / 2;
    float width = M_PI/3;
    for (uint i=0; i<visionNeurons; i++) {
      simu::P2D pl = fromPolar(a + width * i / visionNeurons, .66);
      add(hidden, pl.x, pl.y, 0.);

      simu::P2D pr = fromPolar(-a - width * i / visionNeurons, .66);
      add(hidden, pr.x, pr.y, 0.);
    }
  }

  // Outputs
  add(outputs,  .0,   -.25, .5); // Motor left
  add(outputs,  .0,    .25, .5); // Motor right
  add(outputs, -.125,  .0,  .5); // Clock
  add(outputs, -.25,   .0,  .5); // Reproduction

  bool noHidden = (hiddenLayers == 0) && (visionNeurons == 0);

  NEAT::Substrate substrate (inputs, hidden, outputs);
  substrate.m_leaky = false;
  substrate.m_query_weights_only = false;

  substrate.m_allow_input_hidden_links = false;
  substrate.m_allow_input_output_links = false;
  substrate.m_allow_hidden_hidden_links = false;
  substrate.m_allow_hidden_output_links = false;
  substrate.m_allow_output_hidden_links = false;
  substrate.m_allow_output_output_links = false;
  substrate.m_allow_looped_hidden_links = false;
  substrate.m_allow_looped_output_links = false;

  substrate.m_allow_input_hidden_links = true;
  substrate.m_allow_input_output_links = noHidden;
  substrate.m_allow_hidden_output_links = true;
  substrate.m_allow_hidden_hidden_links = false;

  substrate.m_hidden_nodes_activation = NEAT::SIGNED_SIGMOID;
  substrate.m_output_nodes_activation = NEAT::UNSIGNED_SIGMOID;

  substrate.m_with_distance = false;

  substrate.m_max_weight_and_bias = genotype::HyperNEAT::config_t::maxWeight();

  return substrate;
}

} // end of namespace simu

namespace NEAT {
#define ASRT(X) assertEqual(lhs.X, rhs.X, deepcopy)

void assertEqual (const Connection &lhs, const Connection &rhs, bool deepcopy) {
  using utils::assertEqual;
  ASRT(m_source_neuron_idx);
  ASRT(m_target_neuron_idx);
  ASRT(m_weight);
  ASRT(m_signal);
  ASRT(m_recur_flag);
  ASRT(m_hebb_rate);
  ASRT(m_hebb_pre_rate);
}


void assertEqual (const Neuron &lhs, const Neuron &rhs, bool deepcopy) {
  using utils::assertEqual;
  ASRT(m_activesum);
  ASRT(m_activation);
  ASRT(m_a);
  ASRT(m_b);
  ASRT(m_timeconst);
  ASRT(m_bias);
  ASRT(m_membrane_potential);
  ASRT(m_activation_function_type);
  ASRT(m_x);
  ASRT(m_y);
  ASRT(m_z);
  ASRT(m_sx);
  ASRT(m_sy);
  ASRT(m_sz);
  ASRT(m_substrate_coords);
  ASRT(m_split_y);
  ASRT(m_type);
  ASRT(m_sensitivity_matrix);
}

void assertEqual (const NeuralNetwork &lhs, const NeuralNetwork &rhs,
                  bool deepcopy) {
  using utils::assertEqual;

  ASRT(m_num_inputs);
  ASRT(m_num_outputs);
  ASRT(m_connections);
  ASRT(m_neurons);
}

#undef ASRT
} // end of namespace NEAT
