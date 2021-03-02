#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QTimer>
#include <QShortcut>
#include <QSettings>

#include "kgd/apt/visu/graphicsviewzoom.h"

#include "mainview.h"
#include "actionsbuilder.hpp"

#include "../visu/config.h"

namespace gui {

// =============================================================================
// == Actions managment

template <typename F>
void MainView::addBoolAction (QMenu *m, const QString &iname,
                              const QString &name, const QString &details,
                              QKeySequence k, F f, bool &v) {

  addAction(m,
            new helpers::BoolAction (name, this, v),
            iname, details, k, f);
}

template <typename F, typename T>
void MainView::addEnumAction (QMenu *m, const QString &iname,
                              const QString &name, const QString &details,
                              QKeySequence k, F f, T &v, T max) {

  addEnumAction(m, iname, name, details, k, f, v, Inc<T>([max] (T v) {
    return (v+1)%max;
  }));
}

template <typename F, typename T>
void MainView::addEnumAction (QMenu *m, const QString &iname,
                              const QString &name, const QString &details,
                              QKeySequence k, F f, T &v) {

  using U = EnumUtils<T>;
  Inc<T> next = [] (T v) -> T {
    auto values = U::values();
    auto it = values.find(v);
    if (it != values.end()) {
      it = std::next(it);
      if (it == values.end()) it = values.begin();
      v = *it;
    }
    return v;
  };

  Fmt<T> format = [] (std::ostream &os, const T &v) -> std::ostream& {
    return os << U::getName(v);
  };

  addEnumAction(m, iname, name, details, k, f, v, next, format);
}

template <typename F, typename T>
void MainView::addEnumAction (QMenu *m, const QString &iname,
                              const QString &name, const QString &details,
                              QKeySequence k, F f, T &v,
                              const Inc<T> &next, const Fmt<T> &format) {

  addAction(m,
            new helpers::EnumAction<T>(name, details, this, v, next, format),
            iname, details, k, f);
}


template <typename F>
void MainView::addAction (QMenu *m, const QString &iname, const QString &name,
                          const QString &details, QKeySequence k, F f) {
  QAction *a = new QAction (name, this);
  addAction(m, a, iname, details, k, f);
}

template <typename F>
void MainView::addAction (QMenu *m, QAction *a, const QString &iname,
                          const QString &details, QKeySequence k, F f) {

  static const QIcon err = style()->standardIcon(QStyle::SP_MessageBoxWarning);
  QIcon i;
  if (!iname.isEmpty()) i = QIcon::fromTheme(iname, err);

  a->setIcon(i);
  a->setShortcut(k);
  a->setShortcutContext(Qt::ApplicationShortcut);

  if (!details.isEmpty())  a->setStatusTip(details);

  connect(a, &QAction::triggered, f);
  m->addAction(a);

  _actions[a->text()] = a;
}

void MainView::buildActions(void) {
  _actions.clear();
//  _mbar->setVisible(false);

  QMenu *mSimu = _mbar->addMenu("Simu");

  QString ssa = "SSAction";
  addAction(mSimu, "", ssa, "", Qt::Key_Space, [this] { toggle(); });
  addAction(mSimu, "media-skip-forward", "Step", "", Qt::Key_N,
            [this] { stepping(!_stepping); });

  addAction(mSimu, "media-seek-forward", "Faster", "",
            Qt::ControlModifier + Qt::Key_PageUp, [this] {
    config::Visualisation::substepsSpeed.ref() =
      std::min(256u, config::Visualisation::substepsSpeed() * 2);
    updateWindowName();
  });
  addAction(mSimu, "media-seek-backward", "Slower", "",
            Qt::ControlModifier + Qt::Key_PageDown, [this] {
    config::Visualisation::substepsSpeed.ref() =
      std::max(config::Visualisation::substepsSpeed() / 2, 1u);
    updateWindowName();
  });


  QMenu *mVisu = _mbar->addMenu("Visu");

  addBoolAction(mVisu, "", "Show all", "",
                Qt::Key_Home, [this] { showAll(); },
                _zoomout);

  addAction(mVisu, "zoom-in", "Zoom in", "",
            Qt::Key_PageDown, [this] {
    config::Visualisation::selectionZoomFactor.ref() /= 2.;
    focusOnSelection();
  });
  addAction(mVisu, "zoom-out", "Zoom out", "",
            Qt::Key_PageUp, [this] {
    config::Visualisation::selectionZoomFactor.ref() *= 2.;
    focusOnSelection();
  });
  addAction(mVisu, "go-next", "Select next", "",
            Qt::Key_Right, [this] { selectNext(); });
  addAction(mVisu, "go-previous", "Select previous", "",
            Qt::Key_Left, [this] { selectPrevious(); });

  addEnumAction(mVisu, "", "BrainDead Selection", "",
                Qt::Key_B, [this] {
    auto bd = config::Visualisation::brainDeadSelection();
    if (selection() && bd != BrainDead::IGNORE)
      selection()->object().brainDead = bool(bd);
  }, config::Visualisation::brainDeadSelection.ref());

  addAction(mVisu, "print", "Print", "",
            Qt::ControlModifier + Qt::Key_P, [this] {
    _simu.render("foo.png");
  });

  QMenu *mGui = _mbar->addMenu("GUI");

  QString sd = "Sploinoid data";
  addAction(mGui, "", sd, "", Qt::Key_C,
            [this, sd] {
    toggleCharacterSheet();
    _actions[sd]->setChecked(_manipulator->isVisible());
  });
  _actions[sd]->setCheckable(true);
  _actions[sd]->setChecked(_manipulator->isVisible());

  addBoolAction(mGui, "", "Animate", "",
                Qt::ShiftModifier + Qt::Key_A, [this] {
    _manipulator->toggleBrainAnimation();
  }, config::Visualisation::animateANN.ref());

  QString st = "Stats";
  addAction(mGui, "", st, "", Qt::Key_S,
            [this, st] {
    _stats->setVisible(!_stats->isVisible());
    _actions[st]->setChecked(_stats->isVisible());
  });
  _actions[st]->setCheckable(true);
  _actions[st]->setChecked(_stats->isVisible());


  QMenu *mDraw = _mbar->addMenu("Draw");

  addBoolAction(mDraw, "", "Grid", "",
                Qt::Key_G, [this] { _simu.graphicsEnvironment()->update(); },
                config::Visualisation::showGrid.ref());

  addBoolAction(mDraw, "", "Inner edges", "",
                Qt::Key_I, [this] { scene()->update(); },
                config::Visualisation::drawInnerEdges.ref());

  addBoolAction(mDraw, "", "Opaque bodies", "",
                Qt::Key_O, [this] { scene()->update(); },
                config::Visualisation::opaqueBodies.ref());

#ifndef NDEBUG
  addBoolAction(mDraw, "", "Ghost mode", "",
                Qt::AltModifier + Qt::Key_O, [this] { scene()->update(); },
                config::Visualisation::ghostMode.ref());
#endif

  addEnumAction(mDraw, "", "Vision",
                "1,3: draw vision cone; 2,4: draw rays; 0: none; "
                "1,2: for selection only; 3,4: for all",
                Qt::Key_V,
                [this] { scene()->update(); },
                config::Visualisation::drawVision.ref(), 5);

  addBoolAction(mDraw, "", "Audition", "",
                Qt::Key_A, [this] { scene()->update(); },
                config::Visualisation::drawAudition.ref());

  addEnumAction(mDraw, "", "Type", "", Qt::Key_Tab,
                [this] { scene()->update(); },
                config::Visualisation::renderType.ref());

#ifndef NDEBUG
  QMenu *mDebug = _mbar->addMenu("Debug");

  addEnumAction(mDebug, "", "Show primitives", "", Qt::Key_D,
                [this] { scene()->update(); },
                config::Visualisation::showCollisionObjects.ref(), 4);
  addBoolAction(mDebug, "", "b2 debug draw", "",
                Qt::ControlModifier + Qt::Key_D,
                [this] {
                  _simu.doDebugDrawNow();
                  scene()->update();
                },
                config::Visualisation::b2DebugDraw.ref());

  addEnumAction(mDebug, "", "fight debug draw", "",
                Qt::ControlModifier + Qt::Key_F,
                [this] {  scene()->update();  },
                config::Visualisation::drawFightingDebug.ref(), 5);

  QMenu *smSplines = mDebug->addMenu("Splines");
  for (uint i=0; i<visu::Critter::SPLINES_COUNT; i++) {
    QMenu *smSplinesI = smSplines->addMenu(QString::number(i));
    addAction(smSplinesI, "", "S" + QString::number(i) + "Center", "",
              Qt::KeypadModifier + (Qt::Key_1+i),
              [this, i] {
      config::Visualisation::debugDraw.flip(i);
      scene()->update();
    });
    addAction(smSplinesI, "", "S" + QString::number(i) + "Spline", "",
              Qt::ShiftModifier + Qt::KeypadModifier + (Qt::Key_1+i),
              [this, i] {
      config::Visualisation::debugDraw.flip(i + visu::Critter::SPLINES_COUNT);
      scene()->update();
    });
    addAction(smSplinesI, "", "S" + QString::number(i) + "Edges", "",
              Qt::ControlModifier + Qt::KeypadModifier + (Qt::Key_1+i),
              [this, i] {
      config::Visualisation::debugDraw.flip(i + 2*visu::Critter::SPLINES_COUNT);
      scene()->update();
    });
  }
#endif
}

// =============================================================================
// == Actions managment

MainView::MainView (visu::GraphicSimulation &simulation,
                    StatsView *stats, QMenuBar *bar)
  : _simu(simulation), _stats(stats), _mbar(bar),
    _running(false), _stepping(false), _zoomout(true) {

  setScene(simulation.scene());
  setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

  setDragMode(QGraphicsView::ScrollHandDrag);

  auto Z = config::Visualisation::viewZoom();
  scale(Z, -Z);

  auto *zoomer = new Graphics_view_zoom(this);
  connect(zoomer, &Graphics_view_zoom::zoomed,
          [this] { _zoomout = false; showAll(); });

  connect(&_stepTimer, &QTimer::timeout, this, &MainView::step);

  _manipulator = new GeneticManipulator(this);
//  _manipulator->show();
  connect(_manipulator, &GeneticManipulator::keyReleased,
          this, &MainView::keyReleaseEvent);

  connect(&_simu, &visu::GraphicSimulation::selectionDeleted,
          this, &MainView::selectNext);
  connect(&_simu, &visu::GraphicSimulation::aborted,
          [] { QApplication::exit(); });

  using J = PersitentJoystick;
  _joystick.setSignalManagedButtons({
    J::START, J::SELECT, J::A, J::X, J::B, J::LB, J::RB,
    J::DPAD_LEFT, J::DPAD_UP, J::DPAD_RIGHT, J::DPAD_DOWN
  });
  connect(&_joystick, &J::buttonChanged, this, &MainView::joystickEvent);

  buildActions();

  static const uint STEP_MS = 1000 / config::Simulation::ticksPerSecond();
  _stepTimer.start(STEP_MS);
}

void MainView::updateWindowName(void) {
  QString title = "Main window ";
  if (_running) {
    title += "(Running";
    auto s = config::Visualisation::substepsSpeed();
    if (s > 1)  title += " x" + QString::number(s);
    title += ")";

  } else  title += "(Paused)";
  window()->setWindowTitle(title);
}

void MainView::start(uint speed) {
  config::Visualisation::substepsSpeed.ref() = speed;

  _running = true;
  _actions["SSAction"]->setText("Stop");
  _actions["SSAction"]->setIcon(QIcon::fromTheme("media-playback-pause"));
  updateWindowName();
}

void MainView::stop (void) {
  _running = false;
  _actions["SSAction"]->setText("Start");
  _actions["SSAction"]->setIcon(QIcon::fromTheme("media-playback-start"));
  updateWindowName();
}

void MainView::toggle(void) {
  if (!_running)
        start();
  else  stop();
  updateWindowName();
}

void MainView::stepping(bool s) {
  _stepping = s;
}

void MainView::step(void) {
  _joystick.update();  // Consume joystick events since last step

  if (!_running & !_stepping)  return;

  if (selection()) {
    // Use joystick status to update controlled critter
    externalCritterControl();
  }

  QRectF prevSelectionRect;
  if (selection()) prevSelectionRect = selection()->sceneBoundingRect();

  uint n = config::Visualisation::substepsSpeed();
  if (_stepping)  n = 1;
  for (uint i=0; i<n; i++) {
    _simu.step();
    emit stepped();

    // TODO
//    if (_simu.currTime().timestamp() == 15) {
//      stop();
//      break;
//    }
  }

  if (selection()) {
    // Keep look at the it
    QRectF newSelectionRect = selection()->sceneBoundingRect();
    if (newSelectionRect != prevSelectionRect)
      focusOnSelection();

    _manipulator->readCurrentStatus();
  }

  _stepping = false;

  if (_simu.finished()) stop();
}

void MainView::joystickEvent (JButton b, PersitentJoystick::Value v) {
//  std::cerr << "Joystick event " << b << " = " << v << std::endl;
  if (!v) return;

  static const auto maybe = [] (MainView *v, const QString &k) {
    QAction *a = v->_actions[k];
    if (a)  a->trigger();
    else
      qDebug() << "Invalid action " << k << " requested. Valid ones are:\n\t"
               << v->_actions.keys();
  };

  switch (b) {
  case JButton::START:  maybe(this, "SSAction"); break;
  case JButton::SELECT: maybe(this, "Sploinoid data"); break;

  case JButton::A:          maybe(this, "Step"); break;

  case JButton::LB:         maybe(this, "Zoom out"); break;
  case JButton::RB:         maybe(this, "Zoom in"); break;

  case JButton::DPAD_LEFT:  maybe(this, "Select previous"); break;
  case JButton::DPAD_RIGHT: maybe(this, "Select next"); break;
  case JButton::DPAD_UP:    maybe(this, "Type");  break;
  case JButton::DPAD_DOWN:  maybe(this, "Vision"); break;
  default:
    std::cerr << "Unhandled button type " << b << "\n";
    break;
  }
}

void MainView::keyReleaseEvent(QKeyEvent *e) {
  if (e->modifiers() == Qt::NoModifier && e->key() == Qt::Key_Alt)
    _mbar->setVisible(!_mbar->isVisible());
}

void MainView::mouseReleaseEvent(QMouseEvent *e) {
  QGraphicsView::mouseReleaseEvent(e);

  visu::Critter *c = nullptr;
  QList<QGraphicsItem*> I = items(e->pos());
  for (QGraphicsItem *i: I) {
    c = dynamic_cast<visu::Critter*>(i);

    if (c) {
//      std::cerr << "Clicked on " << simu::CID(c->object()) << " at "
//                << c->x() << "," << c->y() << ": " << c->object().genotype()
//                << std::endl;
      break;
    }
  }

  if (Qt::NoModifier == e->modifiers())
    selectionChanged(c);

  if (c && Qt::ShiftModifier == e->modifiers())
    c->printPhenotype("last.png");
}

void MainView::mouseMoveEvent(QMouseEvent *e) {
  QPointF p = mapToScene(e->pos());
  QString s = QString::number(p.x()) + " x " + QString::number(p.y());
  _simu.statusBar()->showMessage(s, 0);
  QGraphicsView::mouseMoveEvent(e);
}

void MainView::resizeEvent(QResizeEvent *e) {
  QGraphicsView::resizeEvent(e);
  if (selection())
    focusOnSelection();
  else if (_zoomout)
    showAll();
}

void MainView::selectNext(void) {
  visu::Critter *newSelection = nullptr;
  if (!_simu.critters().empty()) {
    auto &critters = _simu.critters();
    if (!selection()) newSelection = critters.begin()->second;
    else {
      auto it = critters.find(&selection()->object());
      assert(it != critters.end());
      if (it == std::prev(critters.end()))
        newSelection = critters.begin()->second;
      else
        newSelection = std::next(it)->second;
    }
  }
  selectionChanged(newSelection);
}

void MainView::selectPrevious(void) {
  visu::Critter *newSelection = nullptr;
  if (!_simu.critters().empty()) {
    auto &critters = _simu.critters();
    if (!selection()) newSelection = std::prev(critters.end())->second;
    else {
      auto it = critters.find(&selection()->object());
      assert(it != critters.end());
      if (it == critters.begin())
        newSelection = std::prev(critters.end())->second;
      else
        newSelection = std::prev(it)->second;
    }
  }

  selectionChanged(newSelection);
}

void MainView::select(visu::Critter *c) {
  selectionChanged(c);
}

void MainView::selectionChanged(visu::Critter *c) {
  static const auto &bd = config::Visualisation::brainDeadSelection();
//  auto q = qDebug();
//  q << "SelectionChanged from "
//    << (selection()? selection()->firstname() : "-1");

  if (selection()) {
    selection()->setSelected(false);
    if (bd != BrainDead::IGNORE) selection()->object().brainDead = false;
    disconnect(selection(), &visu::Critter::shapeChanged,
               _manipulator, &GeneticManipulator::updateShapeData);
  }

  _simu.setSelection(c);

  if (c) {
    selection()->setSelected(true);
    if (bd != BrainDead::IGNORE) selection()->object().brainDead = bool(bd);
    focusOnSelection();

    connect(selection(), &visu::Critter::shapeChanged,
            _manipulator, &GeneticManipulator::updateShapeData);

  } else if (_zoomout)
    showAll();

  _manipulator->setSubject(c);

//  q << "to"
//    << (selection()? selection()->firstname() : "-1")
//    << (void*)selection();
}

void MainView::focusOnSelection (void) {
  if (!selection()) return;

  float S = config::Visualisation::selectionZoomFactor();
  QRectF r = selection()->critterBoundingRect().translated(selection()->pos());
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

void MainView::showAll(void) {
  if (_zoomout)
    fitInView(_simu.graphicsEnvironment()->boundingRect(),
              Qt::KeepAspectRatio);
  _actions["Show all"]->setChecked(_zoomout);
}

void MainView::externalCritterControl(void) {
  static const std::map<PersitentJoystick::MyOwnControllerMapping, Motor>
    j2m_map {
      { PersitentJoystick::AXIS_L_Y, Motor::LEFT  },
      { PersitentJoystick::AXIS_R_Y, Motor::RIGHT },
  };

  for (const auto &p: j2m_map) {
    float v = _joystick.axisValue(p.first);
    if (!isnan(v))  selection()->object().setMotorOutput(-v, p.second);
//    std::cerr << ""
  }

  float lt = .5*(1+_joystick.axisValue(PersitentJoystick::AXIS_L_T)),
        rt = .5*(1+_joystick.axisValue(PersitentJoystick::AXIS_R_T)),
        s = .5;
  if (rt > 0)
    s = .5*(1+rt);
  else if (lt > 0)
    s = .5*(1-lt);
  selection()->object().clockSpeed(s);
}

void MainView::toggleCharacterSheet(void) {
  _manipulator->setVisible(!_manipulator->isVisible());
  QWidget *w = window();
  QRect r = w->frameGeometry();
  if (r.left() < _manipulator->width())
    _manipulator->move(r.right(), _manipulator->y());
  else
    _manipulator->move(r.left() - _manipulator->width(), _manipulator->y());
}

} // end of namespace gui
