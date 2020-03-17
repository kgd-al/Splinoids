#include <QKeyEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QTimer>

#include "kgd/apt/visu/graphicsviewzoom.h"

#include "mainview.h"

#include "geneticmanipulator.h"
#include "../visu/config.h"

#include "../../joystick/joystick.hh"

namespace gui {

class PersitentJoystick {
  using Type = decltype(JoystickEvent::type);
  using Control = decltype(JoystickEvent::number);
  using Value = decltype(JoystickEvent::value);
  std::map<Type, std::map<Control, Value>> mappings;

  Joystick low_level_object;
public:

  enum MyOwnControllerMapping : Control { // Which won't work for you
//    AXIS_L_X = ?,
    AXIS_L_Y = 1,

//    AXIS_R_X = ?,
    AXIS_R_Y = 4
  };

  void update (void) {
    JoystickEvent event;
    while (low_level_object.sample(&event)) {
      if (event.type == JS_EVENT_INIT)  continue;

      mappings[event.type][event.number] = event.value;
    }
  }

  float axisValue (MyOwnControllerMapping c) {
    return value(JS_EVENT_AXIS, c);
  }

  float value (Type t, Control c) {
    auto it = mappings.find(t);
    if (it == mappings.end()) return NAN;

    auto it2 = it->second.find(c);
    if (it2 == it->second.end()) return NAN;

    float v = it2->second;
    if (t == JS_EVENT_AXIS) v /= JoystickEvent::MAX_AXES_VALUE;

    return v;
  }
};
PersitentJoystick joystick;

MainView::MainView (visu::GraphicSimulation &simulation, StatsView *stats)
  : _simu(simulation), _stats(stats), _selection(nullptr) {

  setScene(simulation.scene());
  setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

  setDragMode(QGraphicsView::ScrollHandDrag);

  auto Z = config::Visualisation::viewZoom();
  scale(Z, -Z);

  new Graphics_view_zoom(this);

  connect(&_stepTimer, &QTimer::timeout, this, &MainView::step);

  _manipulator = new GeneticManipulator(this);
//  _manipulator->show();
  connect(_manipulator, &GeneticManipulator::keyReleased,
          this, &MainView::keyReleaseEvent);
}

void MainView::start(void) {
  static const uint STEP_MS = 1000 / config::Simulation::ticksPerSecond();
  _stepTimer.start(STEP_MS);
}

void MainView::stop (void) {
  _stepTimer.stop();
}

void MainView::step(void) {
  joystick.update();  // Consume joystick events since last step

  QPointF prevSelectionPos;
  if (_selection) prevSelectionPos = _selection->pos();

  _simu.step();

  if (_selection) {
    QPointF newSelectionPos = _selection->pos();
    if (newSelectionPos != prevSelectionPos)
      focusOnSelection();
  }

  if (_selection)  // Use joystick status to update controlled critter (if any)
    externalCritterControl();
}

bool MainView::event(QEvent *e) {
  return QGraphicsView::event(e);
}

void MainView::keyReleaseEvent(QKeyEvent *e) {
  if (Qt::KeypadModifier == e->modifiers()) {
    auto k = e->key();
#ifndef NDEBUG
    if (Qt::Key_1 <= k && k <= Qt::Key_4) {
      int i = k - Qt::Key_1;
      if (e->modifiers() & Qt::ControlModifier)
        i += visu::Critter::SPLINES_COUNT;
      config::Visualisation::debugDraw.flip(i);
      std::cerr << config::Visualisation::debugDraw;
      scene()->update();
    } else
#endif
      switch (k) {
      case Qt::Key_Plus:
        config::Visualisation::zoomFactor.ref() /= 2.;
        focusOnSelection();
        break;
      case Qt::Key_Minus:
        config::Visualisation::zoomFactor.ref() *= 2.;
        focusOnSelection();
        break;
      }
  }

  if ((Qt::KeypadModifier | Qt::ControlModifier) == e->modifiers()) {
    switch (e->key()) {
    case Qt::Key_Plus:
      config::Visualisation::substepsSpeed.ref() =
        std::min(256u, config::Visualisation::substepsSpeed() * 2);
      std::cerr << "New speed: " << config::Visualisation::substepsSpeed()
                << std::endl;
      break;
    case Qt::Key_Minus:
      config::Visualisation::substepsSpeed.ref() =
        std::max(config::Visualisation::substepsSpeed() / 2, 1u);
      std::cerr << "New speed: " << config::Visualisation::substepsSpeed()
                << std::endl;
      break;
    }
  }

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
    case Qt::Key_V:
      config::Visualisation::drawVision.ref() =
          (config::Visualisation::drawVision()+1)%3;
      scene()->update();
      break;

#ifndef NDEBUG
    case Qt::Key_D:
      config::Visualisation::showCollisionObjects.ref() =
          (config::Visualisation::showCollisionObjects()+1)%4;
      scene()->update();
      break;
#endif

    case Qt::Key_G:
      _manipulator->setVisible(!_manipulator->isVisible());
      break;
    case Qt::Key_Space:
      if (_stepTimer.isActive())
            stop();
      else  start();
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
#ifndef NDEBUG
    case Qt::Key_D: {
      bool &b2dd = config::Visualisation::b2DebugDraw.ref();
      b2dd = !b2dd;
      _simu.doDebugDrawNow();
      scene()->update();
      break;
    }
#endif

    case Qt::Key_Left:
      selectPrevious();
      break;
    case Qt::Key_Right:
      selectNext();
      break;
    }
  }
}

void MainView::mouseReleaseEvent(QMouseEvent *e) {
  QGraphicsView::mouseReleaseEvent(e);

  QGraphicsItem *i = itemAt(e->pos());
  visu::Critter *c = nullptr;
  if (i)   c = dynamic_cast<visu::Critter*>(i);

//  if (c)
//    std::cerr << "Clicked critter " << c->object().id() << " at "
//              << c->x() << "," << c->y() << ": " << c->object().genotype()
//              << std::endl;

  if (Qt::NoModifier == e->modifiers())
    selectionChanged(c);

  if (c && Qt::ShiftModifier == e->modifiers())
    c->save();
}

void MainView::mouseMoveEvent(QMouseEvent *e) {
  QPointF p = mapToScene(e->pos());
  QString s = QString::number(p.x()) + " x " + QString::number(p.y());
  _simu.statusBar()->showMessage(s, 0);
  QGraphicsView::mouseMoveEvent(e);
}

void MainView::selectNext(void) {
  visu::Critter *newSelection = nullptr;
  auto &critters = _simu.critters();
  if (!_selection)  newSelection = critters.begin()->second;
  else {
    auto it = critters.find(&_selection->object());
    if (it == critters.end()) it = critters.begin();
    else
      newSelection = std::next(it)->second;
  }
  selectionChanged(newSelection);
}

void MainView::selectPrevious(void) {
  visu::Critter *newSelection = nullptr;
  auto &critters = _simu.critters();
  if (!_selection)  newSelection = std::prev(critters.end())->second;
  else {
    auto it = critters.find(&_selection->object());
    if (it == critters.begin()) it = critters.end();
    else
      newSelection = std::prev(it)->second;
  }
  selectionChanged(newSelection);
}

void MainView::selectionChanged(visu::Critter *c) {
//  auto q = qDebug();
//  q << "SelectionChanged from "
//    << (_selection? int(_selection->object().id()) : -1);

  if (_selection) _selection->setSelected(false);
  _selection = c;

  if (c) {
    _selection->setSelected(true);
    focusOnSelection();
  }

  _manipulator->setSubject(c);

//  q << "to"
//    << (_selection? int(_selection->object().id()) : -1);
}

void MainView::focusOnSelection (void) {
  float S = config::Visualisation::zoomFactor();
  QRectF r = _selection->boundingRect().translated(_selection->pos());
//  qDebug() << r;
  if (S != 1) {
    QPointF d (.5*S*r.width(), .5*S*r.height());
    QPointF center = r.center();
    r.setTopLeft(center - d);
    r.setBottomRight(center + d);
  }
//  qDebug() << ">> " << r;
  fitInView(r, Qt::KeepAspectRatio);
}

void MainView::externalCritterControl(void) {
  static const std::map<PersitentJoystick::MyOwnControllerMapping, Motor>
    j2m_map {
      { PersitentJoystick::AXIS_L_Y, Motor::LEFT  },
      { PersitentJoystick::AXIS_R_Y, Motor::RIGHT },
  };

  for (const auto &p: j2m_map) {
    float v = joystick.value(JS_EVENT_AXIS, p.first);
    if (!isnan(v))  _selection->object().setMotorOutput(-v, p.second);
  }
}

} // end of namespace gui
