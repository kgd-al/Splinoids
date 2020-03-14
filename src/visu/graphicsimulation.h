#ifndef GRAPHICSSIMULATION_H
#define GRAPHICSSIMULATION_H

#include <QGraphicsScene>
#include <QStatusBar>

#include "../simu/simulation.h"
#include "environment.h"
#include "critter.h"

namespace visu {

class GraphicSimulation : public simu::Simulation {
  std::map<const simu::Critter*, visu::Critter*> _critters;
  std::unique_ptr<QGraphicsScene> _scene;
  Environment *_graphicEnvironment;

  QStatusBar *_sbar;

public:
  GraphicSimulation(QStatusBar *sbar);
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

  simu::Critter *addCritter(const CGenome &genome,
                            float x, float y, float r) override;
  void delCritter (simu::Critter *critter) override;

  void postInit (void) override;

#ifndef NDEBUG
  void doDebugDrawNow (void) {
    _graphicEnvironment->doDebugDraw();
  }
#endif
};

} // end of namespace visu

#endif // GRAPHICSSIMULATION_H
