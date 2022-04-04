#include <QToolButton>

#include "../../visu/graphicsimulation.h"
#include "../../gui/geneticmanipulator.h"

using Genome = genotype::Critter;

struct SimulationPlaceholder : public visu::GraphicSimulation {
  gui::GeneticManipulator editor;

  SimulationPlaceholder (void);

  void init (void);

  visu::Critter* wmMakeCritter (const Genome &genome);
};

struct SplinoidButton : public QToolButton {
  Q_OBJECT
public:
  static constexpr auto M = 10;
  visu::Critter *critter;

  SplinoidButton (void);

  void resizeEvent(QResizeEvent *e) override;

  void setCritter (visu::Critter *c);

  QSize sizeHint (void) const override;

  void paintEvent (QPaintEvent *e);

signals:
  void focused (void);

protected:
  void focusInEvent(QFocusEvent *e);
};

#ifdef USE_DIMORPHISM
struct Splinoids : public QWidget {
  Q_OBJECT
public:
  SplinoidButton *male, *female;

  Splinoids (void);

  void setGenome (SimulationPlaceholder &s, const Genome &g);

  void setFocus (bool female = true);

  void paintEvent(QPaintEvent *e);
};
#else
struct Splinoids : public QWidget {
  Q_OBJECT
public:
  SplinoidButton *button;

  Splinoids (void);

  void setGenome (SimulationPlaceholder &s, const Genome &g);

  void setFocus (bool = true);
};
#endif
