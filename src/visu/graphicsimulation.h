#ifndef GRAPHICSSIMULATION_H
#define GRAPHICSSIMULATION_H

#include <QGraphicsScene>
#include <QStatusBar>
#include <QLabel>

#include "../simu/simulation.h"
#include "environment.h"
#include "critter.h"
#include "plant.h"

namespace gui {
struct StatsView;
}

namespace visu {

class GraphicSimulation : public simu::Simulation {
  std::map<const simu::Critter*, visu::Critter*> _critters;
  std::map<const simu::Plant*, visu::Plant*> _plants;
  std::unique_ptr<QGraphicsScene> _scene;
  Environment *_graphicEnvironment;

  QStatusBar *_sbar;
  gui::StatsView *_stats;
  QLabel *_timeLabel, *_genLabel;

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

  auto bounds (void) const {
    return _graphicEnvironment->boundingRect();
  }

  void step (void) override;

  simu::Critter* addCritter(const CGenome &genome,
                            float x, float y, float e) override;
  void delCritter (simu::Critter *critter) override;

  simu::Plant* addPlant(float x, float y, float s, float e) override;
  void delPlant(simu::Plant *plant) override;

  void postInit (void) override;

#ifndef NDEBUG
  void doDebugDrawNow (void) {
    _graphicEnvironment->doDebugDraw();
  }
#endif
};

} // end of namespace visu

#endif // GRAPHICSSIMULATION_H
