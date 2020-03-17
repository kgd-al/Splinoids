#ifndef MAINVIEW_H
#define MAINVIEW_H

#include <QGraphicsView>

#include "../visu/graphicsimulation.h"

#include "statsview.h"

namespace gui {

struct GeneticManipulator;

class MainView : public QGraphicsView {
  QTimer _stepTimer;

  visu::GraphicSimulation &_simu;
  GeneticManipulator *_manipulator;

  StatsView *_stats;

  visu::Critter *_selection;

public:
  MainView (visu::GraphicSimulation &simulation, StatsView *stats);

  void keyReleaseEvent(QKeyEvent *e) override;

  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;

  bool event(QEvent* ev) override;

  void selectNext (void);
  void selectPrevious (void);

  void start (void);
  void stop (void);
  void step (void);

private:
  void selectionChanged (visu::Critter *c);
  void focusOnSelection (void);

  void externalCritterControl (void);
};

} // end of namespace gui

#endif // MAINVIEW_H
