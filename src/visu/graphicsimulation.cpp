#include "graphicsimulation.h"
#include "config.h"
#include "b2debugdrawer.h"

#include <QDebug>

namespace visu {

GraphicSimulation::GraphicSimulation(QStatusBar *sbar)
  : _scene (new QGraphicsScene), _sbar(sbar) {}

GraphicSimulation::~GraphicSimulation (void) = default;

void GraphicSimulation::postInit(void) {
  Simulation::postInit();

  _graphicEnvironment = new Environment (environment());
  _scene->addItem(_graphicEnvironment);
  _scene->setSceneRect(_graphicEnvironment->boundingRect());

#ifndef NDEBUG
  _scene->addItem(_graphicEnvironment->debugDrawer());
#endif
}

void GraphicSimulation::step (void) {
  for (uint i=0; i<config::Visualisation::substepsSpeed(); i++)
    Simulation::step();

  for (const auto &p: _critters)
    if (p.first->body().IsAwake())
      p.second->updatePosition();

#ifndef NDEBUG
  if (config::Visualisation::b2DebugDraw()) doDebugDrawNow();
#endif
}

simu::Critter*
GraphicSimulation::addCritter (const CGenome &genome,
                               float x, float y, float r) {

  auto *sc = Simulation::addCritter(genome, x, y, r);
  assert(sc);

  auto *vc = new Critter (*sc);
  assert(vc);

  _scene->addItem(vc);
  _critters.emplace(sc, vc);

  return sc;
}

void GraphicSimulation::delCritter (simu::Critter *critter) {
  Simulation::delCritter(critter);
  auto it = _critters.find(critter);
  assert(it != _critters.end());

  _scene->removeItem(it->second);
  _critters.erase(it);
}

} // end of namespace visu
