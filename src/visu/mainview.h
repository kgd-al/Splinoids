#ifndef MAINVIEW_H
#define MAINVIEW_H

#include <QGraphicsView>

#include "graphicsimulation.h"

namespace visu {

struct GeneticManipulator;

class MainView : public QGraphicsView {
  GraphicSimulation &_simu;
  GeneticManipulator *_manipulator;

  Critter *_selection;

public:
  MainView (GraphicSimulation &simulation);

  void keyReleaseEvent(QKeyEvent *e) override;

  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;

private:
  void selectionChanged (Critter *c);

  void debugTriggerRepop (simu::InitType type,
                          const genotype::Critter &base = genotype::Critter());
};

} // end of namespace visu

#endif // MAINVIEW_H
