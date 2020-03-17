#include "graphicsimulation.h"
#include "config.h"
#include "b2debugdrawer.h"
#include "../gui/statsview.h"

#include <QDebug>

namespace visu {

GraphicSimulation::GraphicSimulation(QStatusBar *sbar, gui::StatsView *stats)
  : _scene (new QGraphicsScene), _sbar(sbar), _stats(stats),
    _timeLabel(new QLabel ("x")), _genLabel(new QLabel ("x")) {
  _sbar->addPermanentWidget(_timeLabel);
  _sbar->addPermanentWidget(_genLabel);
}

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

  _timeLabel->setText(QString::number(_time.timestamp()) + " "
                      + QString::fromStdString(_time.pretty()));
  _genLabel->setText("[" + QString::number(_minGen) + ";"
                     + QString::number(_maxGen) + "]");

  _stats->update("Critters", _critters.size());
  _stats->update("Plants", _plants.size());
}

simu::Critter*
GraphicSimulation::addCritter (const CGenome &genome,
                               float x, float y, float e) {

  auto *sc = Simulation::addCritter(genome, x, y, e);
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

simu::Plant* GraphicSimulation::addPlant(float x, float y, float s, float e) {
  auto *sp = Simulation::addPlant(x, y, s, e);
  assert(sp);

  auto *vp = new Plant (*sp);
  assert(vp);

  _scene->addItem(vp);
  _plants.emplace(sp, vp);

  return sp;
}

void GraphicSimulation::delPlant(simu::Plant *plant) {
  Simulation::delPlant(plant);
  auto it = _plants.find(plant);
  assert(it != _plants.end());

  _scene->removeItem(it->second);
  _plants.erase(it);
}

} // end of namespace visu
