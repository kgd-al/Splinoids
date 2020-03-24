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

  std::map<const simu::Critter*, visu::Critter*> _critters;
  std::map<const simu::Foodlet*, visu::Foodlet*> _foodlets;
  std::unique_ptr<QGraphicsScene> _scene;
  Environment *_graphicEnvironment;

  QStatusBar *_sbar;
  gui::StatsView *_stats;
  QLabel *_timeLabel, *_genLabel;

  visu::Critter *_selection;

public:
  GraphicSimulation(QStatusBar *sbar, gui::StatsView *stats);
  ~GraphicSimulation(void);

  auto scene (void) {
    return _scene.get();
  }

  const auto scene (void) const {
    return _scene.get();
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

  const visu::Foodlet* visuFoodlet (const simu::Foodlet *f) const {
    return _foodlets.at(f);
  }

  auto bounds (void) const {
    return _graphicEnvironment->boundingRect();
  }

  void step (void) override;

  simu::Critter* addCritter(const CGenome &genome,
                            float x, float y, float a, float e) override;
  void delCritter (simu::Critter *critter) override;

  simu::Foodlet* addFoodlet(simu::BodyType t, float x, float y,
                            float s, float e) override;
  void delFoodlet(simu::Foodlet *foodlet) override;

  void postInit (void) override;

#ifndef NDEBUG
  void doDebugDrawNow (void) {
    _graphicEnvironment->doDebugDraw();
  }
#endif

  visu::Critter* selection (void) {
    return _selection;
  }

  void setSelection (visu::Critter *c) {
    _selection = c;
  }

signals:
  void selectionDeleted (void);

private:
  void processStats(const Stats &s) const override;
};

} // end of namespace visu

#endif // GRAPHICSSIMULATION_H
