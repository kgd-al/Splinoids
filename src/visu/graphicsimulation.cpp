#include "graphicsimulation.h"
#include "config.h"
#include "b2debugdrawer.h"
#include "../gui/statsview.h"

#include <QDebug>

namespace visu {

GraphicSimulation::GraphicSimulation(QStatusBar *sbar, gui::StatsView *stats)
  : _scene (new QGraphicsScene), _sbar(sbar), _stats(stats),
    _timeLabel(new QLabel ("Not started yet")), _genLabel(new QLabel ("")),
    _selection(nullptr) {

  _sbar->addPermanentWidget(_timeLabel);
  _sbar->addPermanentWidget(_genLabel);

  _stats->setupFields({
    "-- Counts --",
    "Critters", "Plants", "Corpses", "Feedings", "Fights",

    "-- Energy --",
    "[E] Plants", "[E] Corpses", "[E] Splinoids", "[E] Reserve",
    "[E] Total",

    "-- Durations --",
    "[D] Visu    ",
     "[D] Simu   ",
      "[D] Spln  ",
      "[D] Env   ",
       "[D] Box2D",
      "[D] Decay ",
      "[D] Regen ",
  });

  _gstepTimeMs = 0;
}

GraphicSimulation::~GraphicSimulation (void) = default;

void GraphicSimulation::postInit(void) {
  Simulation::postInit();

  _graphicEnvironment =
    new Environment (environment(),
                     [this] (const simu::Critter *c) { return visuCritter(c); },
                     [this] (const simu::Foodlet *f) { return visuFoodlet(f); }
  );

  _scene->addItem(_graphicEnvironment);
  _scene->addItem(_graphicEnvironment->overlay());
  _scene->setSceneRect(_graphicEnvironment->boundingRect());

#ifndef NDEBUG
  _scene->addItem(_graphicEnvironment->debugDrawer());
#endif
}

void GraphicSimulation::step (void) {
  auto start = now();

  for (uint i=0; i<config::Visualisation::substepsSpeed(); i++)
    Simulation::step();

  for (const auto &p: _critters)
    if (p.first->body().IsAwake())
      p.second->updatePosition();

  for (const auto &p: _foodlets)
    if (p.first->isCorpse())
      p.second->update();

#ifndef NDEBUG
  if (config::Visualisation::b2DebugDraw()) doDebugDrawNow();
#endif

  _timeLabel->setText(QString::number(_time.timestamp()) + " "
                      + QString::fromStdString(_time.pretty()));
  _genLabel->setText("[" + QString::number(_minGen) + ";"
                     + QString::number(_maxGen) + "]");

  _gstepTimeMs = durationFrom(start);
}

void GraphicSimulation::processStats(const Stats &s) const {
  _stats->update("Critters", s.ncritters, 0);
  _stats->update("Plants", s.nplants, 0);
  _stats->update("Corpses", s.ncorpses, 0);

  _stats->update("Feedings", s.nfeedings, 0);
  _stats->update("Fights", s.nfights, 0);

  _stats->update("[E] Plants", float(s.eplants), 2);
  _stats->update("[E] Corpses", float(s.ecorpses), 2);

  _stats->update("[E] Splinoids", float(s.ecritters), 2);

  _stats->update("[E] Reserve", float(s.ereserve), 2);
  _stats->update("[E] Total",
                 float(s.eplants+s.ecorpses+s.ecritters+s.ereserve), 2);

  _stats->update("[D] Visu    ", _gstepTimeMs, 0);
  _stats->update( "[D] Simu   ", _stepTimeMs, 0);
  _stats->update(  "[D] Spln  ", _splnTimeMs, 0);
  _stats->update(  "[D] Env   ", _envTimeMs, 0);
  _stats->update(   "[D] Box2D", _environment->physics().GetProfile().step, 0);
  _stats->update(  "[D] Decay ", _envTimeMs, 0);
  _stats->update(  "[D] Regen ", _envTimeMs, 0);
}

simu::Critter*
GraphicSimulation::addCritter (const CGenome &genome,
                               float x, float y, float a, simu::decimal e) {

  auto *sc = Simulation::addCritter(genome, x, y, a, e);
  assert(sc);

  auto *vc = new Critter (*sc);
  assert(vc);

  _scene->addItem(vc);
  _critters.emplace(sc, vc);

  return sc;
}

void GraphicSimulation::delCritter (simu::Critter *critter) {
  auto it = _critters.find(critter);
  assert(it != _critters.end());

  visu::Critter *vcritter = it->second;
  _scene->removeItem(vcritter);
  _critters.erase(it);

  if (_selection && critter == &_selection->object()) {
    _selection = nullptr;
    emit selectionDeleted();
  }

  delete vcritter;
  Simulation::delCritter(critter);
}

simu::Foodlet*
GraphicSimulation::addFoodlet(simu::BodyType t,
                              float x, float y, float s, simu::decimal e) {
  auto *sf = Simulation::addFoodlet(t, x, y, s, e);
  assert(sf);

  auto *vf = new Foodlet (*sf);
  assert(vf);

  _scene->addItem(vf);
  _foodlets.emplace(sf, vf);

  return sf;
}

void GraphicSimulation::delFoodlet(simu::Foodlet *foodlet) {
  auto it = _foodlets.find(foodlet);
  assert(it != _foodlets.end());

  visu::Foodlet *vfoodlet= it->second;
  _scene->removeItem(it->second);
  _foodlets.erase(it);

  delete vfoodlet;
  Simulation::delFoodlet(foodlet);
}

} // end of namespace visu
