#include <QResizeEvent>
#include <QHBoxLayout>
#include <QPainter>

#include "watchmaker_classes.h"
#include "../../visu/config.h"

#include <QDebug>

SimulationPlaceholder::SimulationPlaceholder (void)
  : visu::GraphicSimulation(nullptr, nullptr) {}

void SimulationPlaceholder::init (void) {
  rng::FastDice dice (0);
  auto genome = genotype::Environment::random(dice);
  _environment = std::make_unique<simu::Environment>(genome);

  _environment->init(0, 0);
}

visu::Critter* SimulationPlaceholder::wmMakeCritter (const Genome &genome) {
  b2Body *body = critterBody(0, 0, 0);

  float age = .5 * (genome.matureAge + genome.oldAge);
  simu::decimal energy =
    simu::Critter::maximalEnergyStorage(simu::Critter::MAX_SIZE);
  simu::Critter *sc = new simu::Critter (genome, body, energy, age);
  simu::Simulation::_critters.insert(sc);

  visu::Critter *vc = new visu::Critter (*sc);
  visu::GraphicSimulation::_critters.emplace(sc, vc);

  return vc;
}


SplinoidButton::SplinoidButton (void) : critter(nullptr) {
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

void SplinoidButton::resizeEvent(QResizeEvent *e) {
//  qDebug() << "Resizing " << this << ": " << e->oldSize() << " >> " << e->size();
  e->accept();
  auto w = e->size().width(), h = e->size().height();
  if(w > h) resize(h, h);
  else      resize(w, w);
}

void SplinoidButton::setCritter (visu::Critter *c) {
  critter = c;
  update();
}

QSize SplinoidButton::sizeHint (void) const {
//  static const float Z = config::Visualisation::viewZoom();
  static const float Z = 10;
  int D = 2;//simu::Critter::MAX_SIZE * simu::Critter::RADIUS;
//    if (critter)  D = critter->object().bodyDiameter();
  int S = Z * D + 2 * M;
  return QSize(S, S);
}

void SplinoidButton::paintEvent (QPaintEvent *e) {
  QToolButton::paintEvent(e);
  QPainter p (this);
  QRect r = rect().adjusted(M, M, -M, -M);
//  p.fillRect(r, Qt::red);

  if (!critter) return;
  auto MR = critter->critterBoundingRect();
  float M = std::max(MR.width(), MR.height());
  float S = r.width() / M;

  p.setRenderHint(QPainter::Antialiasing, true);
  p.translate(.5*rect().height(), .5*rect().width());
  p.rotate(-90);
  p.scale(S, S);
  critter->doPaint(&p);
}

void SplinoidButton::focusInEvent(QFocusEvent *e) {
  QToolButton::focusInEvent(e);
  emit focused();
}

#ifdef USE_DIMORPHISM
Splinoids::Splinoids (void) {
  male = new SplinoidButton;
  female = new SplinoidButton;

  QHBoxLayout *layout = new QHBoxLayout;
  layout->addWidget(male);
  layout->addWidget(female);
  layout->setSpacing(0);
  setLayout(layout);
}

void Splinoids::paintEvent(QPaintEvent*) {
  QPainter p (this);
  p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void Splinoids::setFocus(bool female) {
  (female ? this->female : this->male)->setFocus();
}

void Splinoids::setGenome (SimulationPlaceholder &s, const Genome &g) {
  Genome gmale = g, gfemale = g;
  gmale.cdata.sex = simu::Critter::Sex::MALE;
  gfemale.cdata.sex = simu::Critter::Sex::FEMALE;

  male->setCritter(s.wmMakeCritter(gmale));
  female->setCritter(s.wmMakeCritter(gfemale));

  phenotype::ANN b = female->critter->object().brain();
  std::cerr << std::setw(4) << b.neurons().size()
            << " neurons " << std::setw(6) << b.stats().edges
            << " edges" << std::endl;
}
#else
Splinoids::Splinoids (void) {
  button = new SplinoidButton;

  QHBoxLayout *layout = new QHBoxLayout;
  layout->addWidget(button);
  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);
  setLayout(layout);
}

void Splinoids::setFocus(bool) {
  button->setFocus();
}

void Splinoids::setGenome (SimulationPlaceholder &s, const Genome &g) {
  button->setCritter(s.wmMakeCritter(g));

  phenotype::ANN b = button->critter->object().brain();
  std::cerr << std::setw(4) << b.neurons().size()
            << " neurons " << std::setw(6) << b.stats().edges
            << " edges" << std::endl;
}
#endif
