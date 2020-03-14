#ifndef MAINVIEW_H
#define MAINVIEW_H

#include <QGraphicsView>

#include "graphicsimulation.h"

namespace visu {

struct GeneticManipulator;

class MainView : public QGraphicsView {
  QTimer _stepTimer;

  GraphicSimulation &_simu;
  GeneticManipulator *_manipulator;

  Critter *_selection;

public:
  static constexpr uint STEP_MS = 1000 * simu::Environment::DT;

  MainView (GraphicSimulation &simulation);

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
  void selectionChanged (Critter *c);
  void focusOnSelection (void);

  void externalCritterControl (void);

  void debugTriggerRepop (simu::InitType type,
                          const genotype::Critter &base = genotype::Critter());
};

} // end of namespace visu

#endif // MAINVIEW_H
