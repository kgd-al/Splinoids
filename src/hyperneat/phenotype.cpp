#include "phenotype.h"

namespace simu {

NEAT::Substrate substrateFor (const std::vector<simu::P2D> &rays) {
  // TODO Move to a function
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
    for (uint i=0; i<3; i++) add(inputs, r_.x, r_.y, -.2*i-.6);
  }

  // Hidden
  uint NH = 5;
  for (uint i=0; i<NH; i++) {
    simu::P2D p = fromPolar(2 * M_PI * i / NH, .25);
    add(hidden, p.x, p.y, 0.);
  }

  // Outputs
  add(outputs,  .0,   -.25, .5); // Motor left
  add(outputs,  .0,    .25, .5); // Motor right
  add(outputs, -.125,  .0,  .5); // Clock
  add(outputs, -.25,   .0,  .5); // Reproduction

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
  substrate.m_allow_input_output_links = false;
  substrate.m_allow_hidden_output_links = true;
  substrate.m_allow_hidden_hidden_links = false;

  substrate.m_hidden_nodes_activation = NEAT::SIGNED_SIGMOID;
  substrate.m_output_nodes_activation = NEAT::UNSIGNED_SIGMOID;

  substrate.m_with_distance = false;

  substrate.m_max_weight_and_bias = 8.0;

  return substrate;
}

} // end of namespace simu
