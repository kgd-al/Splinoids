#include "graphicsimulation.h"
#include "config.h"
#include "b2debugdrawer.h"
#include "../gui/statsview.h"

#include <QPainter>

#include <QDebug>

namespace visu {

GraphicSimulation::GraphicSimulation(void)
  : GraphicSimulation(nullptr, nullptr) {}

GraphicSimulation::GraphicSimulation(QStatusBar *sbar, gui::StatsView *stats)
  : _scene (new QGraphicsScene), _sbar(sbar), _stats(stats),
    _timeLabel(new QLabel ("Not started yet")), _genLabel(new QLabel ("")),
    _selection(nullptr) {

  if (_sbar) {
    _sbar->addPermanentWidget(_timeLabel);
    _sbar->addPermanentWidget(_genLabel);
  }

  if (_stats)
    _stats->setupFields({
      "-- Counts --",
      "Critters",
       "Youngs ",
       "Adults ",
       "Elders ",
      "Plants", "Corpses", "Feedings", "Fights",

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
  setupVisuEnvironment();
}

void GraphicSimulation::setupVisuEnvironment(void) {
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

  Simulation::step();

  for (const auto &p: _critters)  p.second->update();

  for (const auto &p: _foodlets)
    if (p.first->isCorpse())
      p.second->update();

#ifndef NDEBUG
  if (config::Visualisation::b2DebugDraw()) doDebugDrawNow();
#endif

  _timeLabel->setText(QString::number(_time.timestamp()) + " "
                      + QString::fromStdString(_time.pretty()));
  _genLabel->setText("[" + QString::number(_genData.min) + ";"
                     + QString::number(_genData.max) + "]");

  _gstepTimeMs = durationFrom(start);
}

void GraphicSimulation::processStats(const Stats &s) const {
  if (!_stats)  return;

  _stats->update("Critters", s.ncritters, 0);
  _stats->update("Plants", s.nplants, 0);
  _stats->update("Corpses", s.ncorpses, 0);

  _stats->update("Youngs ", s.nyoungs, 0);
  _stats->update("Adults ", s.nadults, 0);
  _stats->update("Elders ", s.nelders, 0);

  _stats->update("Feedings", s.nfeedings, 0);
  _stats->update("Fights", s.nfights, 0);

  _stats->update("[E] Plants", float(s.eplants), 2);
  _stats->update("[E] Corpses", float(s.ecorpses), 2);

  _stats->update("[E] Splinoids", float(s.ecritters), 2);

  _stats->update("[E] Reserve", float(s.ereserve), 2);
  _stats->update("[E] Total",
                 float(s.eplants+s.ecorpses+s.ecritters+s.ereserve), 2);

  const auto &st = _timeMs;
  _stats->update("[D] Visu    ", _gstepTimeMs, 0);
  _stats->update( "[D] Simu   ", st.step, 0);
  _stats->update(  "[D] Spln  ", st.spln, 0);
  _stats->update(  "[D] Env   ", st.env, 0);
  _stats->update(   "[D] Box2D", _environment->physics().GetProfile().step, 0);
  _stats->update(  "[D] Decay ", st.decay, 0);
  _stats->update(  "[D] Regen ", st.regen, 0);
}

void GraphicSimulation::addVisuCritter(simu::Critter *sc) {
  assert(sc);

  auto *vc = new Critter (*sc);
  assert(vc);

  _scene->addItem(vc);
  _critters.emplace(sc, vc);
}

simu::Critter*
GraphicSimulation::addCritter (const CGenome &genome,
                               float x, float y, float a, simu::decimal e,
                               float age) {

  auto *sc = Simulation::addCritter(genome, x, y, a, e, age);
  addVisuCritter(sc);
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

void GraphicSimulation::addVisuFoodlet(simu::Foodlet *sf) {
  assert(sf);

  auto *vf = new Foodlet (*sf);
  assert(vf);

  _scene->addItem(vf);
  _foodlets.emplace(sf, vf);
}

simu::Foodlet*
GraphicSimulation::addFoodlet(simu::BodyType t,
                              float x, float y, float s, simu::decimal e) {
  auto *sf = Simulation::addFoodlet(t, x, y, s, e);
  addVisuFoodlet(sf);
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

QPixmap GraphicSimulation::render (void) const {
  static const float Z = config::Visualisation::viewZoom();
  QRectF r = _scene->sceneRect();
  QPixmap p (r.width() * Z, r.height() * Z);
  QPainter painter (&p);
  painter.setRenderHint(QPainter::Antialiasing, true);

  _scene->render(&painter);

  return p;
}

void GraphicSimulation::render (const QString &filename) const {
  render().save(filename);
}

void GraphicSimulation::load (const std::string &file, GraphicSimulation &s,
                              const std::string &constraints,
                              const std::string &fields) {

  Simulation::load(file, s, constraints, fields);

  for (simu::Critter *c: s.Simulation::_critters) s.addVisuCritter(c);
  for (simu::Foodlet *f: s.Simulation::_foodlets) s.addVisuFoodlet(f);
  s.setupVisuEnvironment();
}

} // end of namespace visu
