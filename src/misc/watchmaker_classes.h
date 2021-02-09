#include <QToolButton>

#include "../visu/graphicsimulation.h"
#include "../gui/geneticmanipulator.h"

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
};

struct Splinoids : public QWidget {
  Q_OBJECT
public:
  SplinoidButton *male, *female;

  Splinoids (void);

  void setGenome (SimulationPlaceholder &s, const Genome &g);

  void paintEvent(QPaintEvent *e);
};
