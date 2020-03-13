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
  Environment *_graphicEnvironment;

  QGraphicsScene *_scene;

  QStatusBar *_sbar;

public:
  GraphicSimulation(QStatusBar *sbar);

  auto scene (void) {
    return _scene;
  }

  const auto scene (void) const {
    return _scene;
  }

  auto statusBar (void) {
    return _sbar;
  }

  const auto& critters (void) const {
    return _critters;
  }

  simu::Critter *addCritter(const CGenome &genome,
                            float x, float y, float r) override;
  void delCritter (simu::Critter *critter) override;

  void postInit (void) override;
};

} // end of namespace visu

#endif // GRAPHICSSIMULATION_H
