#ifndef GRAPHICSSIMULATION_H
#define GRAPHICSSIMULATION_H

#include <QGraphicsScene>
#include <QStatusBar>
#include <QLabel>

#include "../simu/simulation.h"
#include "environment.h"
#include "critter.h"
#include "foodlet.h"

namespace gui {
struct StatsView;
}

namespace visu {

class GraphicSimulation : public QObject, public simu::Simulation {
  Q_OBJECT
protected:
  std::map<const simu::Critter*, visu::Critter*> _critters;
  std::map<const simu::Foodlet*, visu::Foodlet*> _foodlets;
  std::unique_ptr<QGraphicsScene> _scene;
  Environment *_graphicEnvironment;

  QStatusBar *_sbar;
  gui::StatsView *_stats;
  QLabel *_timeLabel, *_genLabel;

  visu::Critter *_selection;

  uint _gstepTimeMs;

public:
  // For use in gui-less environments (but still with rendering)
  GraphicSimulation (void);

  // For regular use (gui-embedded)
  GraphicSimulation (QStatusBar *sbar, gui::StatsView *stats);

  ~GraphicSimulation(void);

  auto scene (void) {
    return _scene.get();
  }

  const auto scene (void) const {
    return _scene.get();
  }

  const auto* graphicsEnvironment (void) const {
    return _graphicEnvironment;
  }

  auto* graphicsEnvironment (void) {
    return _graphicEnvironment;
  }

  auto statusBar (void) {
    return _sbar;
  }

  const auto& critters (void) const {
    return _critters;
  }

  auto& critters (void) {
    return _critters;
  }

  const visu::Critter* visuCritter (const simu::Critter *c) const {
    return _critters.at(c);
  }

  visu::Critter* visuCritter (const simu::Critter *c) {
    return _critters.at(c);
  }

  const visu::Foodlet* visuFoodlet (const simu::Foodlet *f) const {
    return _foodlets.at(f);
  }

  visu::Foodlet* visuFoodlet (const simu::Foodlet *f) {
    return _foodlets.at(f);
  }

  auto bounds (void) const {
    return _graphicEnvironment->boundingRect();
  }

  simu::Critter* addCritter(CGenome genome,
                            float x, float y, float a,
                            simu::decimal e,
                            float age, bool overrideGID,
                            const phenotype::ANN *brainTemplate) override;
  void delCritter (simu::Critter *critter) override;

  simu::Foodlet* addFoodlet(simu::BodyType t, float x, float y,
                            float s, simu::decimal e) override;
  void delFoodlet(simu::Foodlet *foodlet) override;

  simu::Obstacle *addObstacle(float x, float y, float w, float h,
                              simu::Color c = simu::Color{-1}) override;

  void postInit (void) override;

  void step (void) override;

#ifndef NDEBUG
  void doDebugDrawNow (void) {
    _graphicEnvironment->doDebugDraw();
  }
#endif

  void abort (void) override {
    Simulation::abort();
    emit aborted();
  }

  visu::Critter* selection (void) {
    return _selection;
  }

  void setSelection (visu::Critter *c) {
    _selection = c;
  }

  void render(QPaintDevice *d, QRectF trect) const;
  void render (QString filename) const;

  static void load (const std::string &file, GraphicSimulation &s,
                    const std::string &constraints,
                    const std::string &fields);

signals:
  void selectionDeleted (void);
  void aborted (void);

private:
  void processStats(const Stats &s) const override;

  void setupVisuEnvironment (void);
  void addVisuCritter (simu::Critter *c);
  void addVisuFoodlet (simu::Foodlet *f);
};

} // end of namespace visu

#endif // GRAPHICSSIMULATION_H
