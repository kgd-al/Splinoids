#include <QKeyEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QTimer>

#include "kgd/apt/visu/graphicsviewzoom.h"

#include "mainview.h"

#include "geneticmanipulator.h"
#include "config.h"

namespace visu {

QTimer timer;
int TIMER_PERIOD = 10000; // ms

MainView::MainView (GraphicSimulation &simulation)
  : _simu(simulation), _selection(nullptr) {

  setScene(simulation.scene());
  setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

  setDragMode(QGraphicsView::ScrollHandDrag);

  auto Z = config::Visualisation::viewZoom();
  scale(Z, -Z);

  new Graphics_view_zoom(this);

  connect(&timer, &QTimer::timeout, [this] {
    debugTriggerRepop(simu::InitType::MEGA_RANDOM);
  });
//  timer.start(TIMER_PERIOD);

  _manipulator = new GeneticManipulator(this);
//  _manipulator->show();
  connect(_manipulator, &GeneticManipulator::keyReleased,
          this, &MainView::keyReleaseEvent);
}

void MainView::keyReleaseEvent(QKeyEvent *e) {

#ifndef NDEBUG
  if (Qt::KeypadModifier & e->modifiers()) {
    auto k = e->key();
    if (Qt::Key_1 <= k && k <= Qt::Key_4) {
      int i = k - Qt::Key_1;
      if (e->modifiers() & Qt::ControlModifier) i += Critter::SPLINES_COUNT;
      config::Visualisation::debugDraw.flip(i);
      std::cerr << config::Visualisation::debugDraw;
      scene()->update();
    }
  }
#endif

  if (Qt::NoModifier == e->modifiers()) {
    switch (e->key()) {
    case Qt::Key_I:
      config::Visualisation::drawInnerEdges.ref() =
          !config::Visualisation::drawInnerEdges();
      scene()->update();
      break;
    case Qt::Key_O:
      config::Visualisation::opaqueBodies.ref() =
          !config::Visualisation::opaqueBodies();
      scene()->update();
      break;

#ifndef NDEBUG
    case Qt::Key_D:
      config::Visualisation::showCollisionObjects.ref() =
          (config::Visualisation::showCollisionObjects()+1)%4;
      scene()->update();
      break;
#endif

    case Qt::Key_R:
      debugTriggerRepop(simu::InitType::RANDOM);
      break;
    case Qt::Key_G:
      _manipulator->setVisible(!_manipulator->isVisible());
      break;
    case Qt::Key_Space:
      if (timer.isActive()) timer.stop();
      else
        timer.start(TIMER_PERIOD);
      qDebug() << "Timer running: " << timer.isActive();
      break;

    case Qt::Key_Tab: {
      using EU_RT = EnumUtils<RenderingType>;
      auto &v = config::Visualisation::renderingType.ref();
      auto values = EU_RT::values();
      auto it = values.find(v);
      if (it != values.end()) {
        it = std::next(it);
        if (it == values.end()) it = values.begin();
        v = *it;
        scene()->update();
        std::cerr << "Rendering type: " << config::Visualisation::renderingType()
                  << std::endl;
      }
      break;
    }
    }

  } else if (Qt::ControlModifier == e->modifiers()) {
    switch (e->key()) {
    case Qt::Key_R:
      debugTriggerRepop(simu::InitType::MEGA_RANDOM);
      break;
    }
  }
}

void MainView::mouseReleaseEvent(QMouseEvent *e) {
  QGraphicsView::mouseReleaseEvent(e);

  QGraphicsItem *i = itemAt(e->pos());
  Critter *c = nullptr;
  if (i)   c = dynamic_cast<Critter*>(i);

//  if (c)
//    std::cerr << "Clicked critter " << c->object().id() << " at "
//              << c->x() << "," << c->y() << ": " << c->object().genotype()
//              << std::endl;

  if (Qt::NoModifier == e->modifiers())
    selectionChanged(c);

  if (c && Qt::ControlModifier == e->modifiers())
    debugTriggerRepop(simu::InitType::MUTATIONAL_LANDSCAPE,
                      c->object().genotype());

  if (c && Qt::ShiftModifier == e->modifiers())
    c->save();
}

void MainView::debugTriggerRepop (simu::InitType type,
                                  const genotype::Critter &base) {

  _manipulator->setSubject(nullptr);
  _simu.clear();
  _simu.init(_simu.environment().genotype(), base, type);

  QRectF crittersBounds;
  for (const auto &pair: _simu.critters()) {
    const Critter *c = pair.second;
    crittersBounds =
      crittersBounds.united(c->boundingRect().translated(c->pos()));
  }

//  qDebug() << "Adjusting view to " << crittersBounds;
  fitInView(crittersBounds, Qt::KeepAspectRatio);
}

void MainView::mouseMoveEvent(QMouseEvent *e) {
  QPointF p = mapToScene(e->pos());
  QString s = QString::number(p.x()) + " x " + QString::number(p.y());
  _simu.statusBar()->showMessage(s, 0);
  QGraphicsView::mouseMoveEvent(e);
}

void MainView::selectionChanged(Critter *c) {
//  auto q = qDebug();
//  q << "SelectionChanged from "
//    << (_selection? int(_selection->object().id()) : -1);

  if (_selection) _selection->setSelected(false);
  _selection = c;

  if (c)  c->setSelected(true);
  _manipulator->setSubject(c);

//  q << "to"
//    << (_selection? int(_selection->object().id()) : -1);
}

} // end of namespace visu
