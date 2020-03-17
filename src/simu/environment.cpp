#include "environment.h"

#include "critter.h"
#include "config.h"

namespace simu {

Environment::Environment(const Genome &g)
  : _genome(g), _physics({0,0}) {
  createEdges();
}

Environment::~Environment (void) {
  _physics.DestroyBody(_edges);
}

void Environment::step (void) {

  // Box2D parameters
  static const float DT = 1.f / config::Simulation::secondsPerDay();
  static const int V_ITER = config::Simulation::b2VelocityIter();
  static const int P_ITER = config::Simulation::b2PositionIter();

  _physics.Step(DT, V_ITER, P_ITER);
//  std::cerr << "Physical step took " << _physics.GetProfile().step
//            << " ms" << std::endl;
}

void Environment::createEdges(void) {
  real HS = extent();
  P2D edgesVertices [4] {
    { HS, HS }, { -HS, HS }, { -HS, -HS }, { HS, -HS }
  };

  b2ChainShape edgesShape;
  edgesShape.CreateLoop(edgesVertices, 4);

  b2BodyDef edgesBodyDef;
  edgesBodyDef.type = b2_staticBody;
  edgesBodyDef.position.Set(0, 0);

  _edges = _physics.CreateBody(&edgesBodyDef);
  _edges->CreateFixture(&edgesShape, 0);
}

} // end of namespace simu
