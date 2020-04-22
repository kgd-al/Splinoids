#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QTimer>
#include <QShortcut>

#include "kgd/apt/visu/graphicsviewzoom.h"

#include "mainview.h"

#include "geneticmanipulator.h"
#include "../visu/config.h"

namespace gui {

// =============================================================================
// == Actions managment

namespace details {

struct BoolAction : public QAction {
  bool &value;
  BoolAction (const QString &name, QWidget *parent, bool &v)
    : QAction(name, parent), value(v) {

    setCheckable(true);
    setChecked(value);

    connect(this, &QAction::triggered, this, &BoolAction::toggle);
  }

  void toggle (void) {
    value = !value;
  }
};

template <typename T>
struct EnumAction : public QAction {
  T &value;
  QString details;

  using Incrementer = std::function<T(T)>;
  const Incrementer incrementer;

  using Formatter = std::function<std::ostream&(std::ostream&, const T&)>;
  const Formatter formatter;

  EnumAction (const QString &name, const QString &details, QWidget *parent,
              T &v, Incrementer i, Formatter f)
    : QAction(name, parent),
      value(v), details(details), incrementer(i), formatter(f) {

    connect(this, &QAction::triggered, this, &EnumAction::next);
    updateStatus();
  }

  void next (void) {
    value = incrementer(value);
    updateStatus();
  }

  void updateStatus (void) {
    std::ostringstream oss;
    oss << details.toStdString()
        << "; Current status for " << text().toStdString() << ": ";
    formatter(oss, value);
    oss << ")";
    setStatusTip(QString::fromStdString(oss.str()));
  }
};

} // end of namespace details

template <typename F>
void MainView::addBoolAction (QMenu *m, const QString &iname,
                              const QString &name, const QString &details,
                              QKeySequence k, F f, bool &v) {

  addAction(m,
            new details::BoolAction (name, this, v),
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
                              QKeySequence k, F f, T &v,
                              const Inc<T> &next, const Fmt<T> &format) {

  addAction(m,
            new details::EnumAction<T>(name, details, this, v, next, format),
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
            Qt::KeypadModifier + Qt::ControlModifier + Qt::Key_Plus, [this] {
    config::Visualisation::substepsSpeed.ref() =
      std::min(256u, config::Visualisation::substepsSpeed() * 2);
    updateWindowName();
  });
  addAction(mSimu, "media-seek-backward", "Slower", "",
            Qt::KeypadModifier + Qt::ControlModifier + Qt::Key_Minus, [this] {
    config::Visualisation::substepsSpeed.ref() =
      std::max(config::Visualisation::substepsSpeed() / 2, 1u);
    updateWindowName();
  });


  QMenu *mVisu = _mbar->addMenu("Visu");

  addBoolAction(mVisu, "", "Show all", "",
                Qt::Key_Home, [this] { showAll(); },
                _zoomout);

  addAction(mVisu, "zoom-in", "Zoom in", "",
            Qt::KeypadModifier + Qt::Key_Plus, [this] {
    config::Visualisation::selectionZoomFactor.ref() /= 2.;
    focusOnSelection();
  });
  addAction(mVisu, "zoom-out", "Zoom out", "",
            Qt::KeypadModifier + Qt::Key_Minus, [this] {
    config::Visualisation::selectionZoomFactor.ref() *= 2.;
    focusOnSelection();
  });
  addAction(mVisu, "go-next", "Select next", "",
            Qt::Key_Right, [this] { selectNext(); });
  addAction(mVisu, "go-previous", "Select previous", "",
            Qt::Key_Left, [this] { selectPrevious(); });

  std::cerr << "brain dead: " << config::Visualisation::brainDeadSelection()
            << std::endl;
  addBoolAction(mVisu, "", "BrainDead Selection", "",
                Qt::Key_B, [this] {
    if (selection()) selection()->object().brainDead =
        config::Visualisation::brainDeadSelection();
  }, config::Visualisation::brainDeadSelection.ref());

  QMenu *mGui = _mbar->addMenu("GUI");

  QString sd = "Sploinoid data";
  addAction(mGui, "", sd, "", Qt::Key_C,
            [this, sd] {
    toggleCharacterSheet();
    _actions[sd]->setChecked(_manipulator->isVisible());
  });
  _actions[sd]->setCheckable(true);
  _actions[sd]->setChecked(_manipulator->isVisible());


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

  addEnumAction(mDraw, "", "Vision",
                "1: draw vision cone; 2: draw rays; 3: both; 0: none",
                Qt::Key_V,
                [this] { scene()->update(); },
                config::Visualisation::drawVision.ref(), 3);

  addEnumAction(mDraw, "", "Type", "", Qt::Key_Tab,
                [this] { scene()->update(); },
                config::Visualisation::renderType.ref(),
                Inc<RenderingType>([] (RenderingType t) {
    using EU_RT = EnumUtils<RenderingType>;
    auto values = EU_RT::values();
    auto it = values.find(t);
    if (it != values.end()) {
      it = std::next(it);
      if (it == values.end()) it = values.begin();
      t = *it;
      std::cerr << "Rendering type: " << t << std::endl;
    }
    return t;

  }), Fmt<RenderingType>(
      [] (std::ostream &os, const RenderingType t) -> std::ostream& {
    return os << EnumUtils<RenderingType>::getName(t);
  }));

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
    addAction(smSplinesI, "", "S" + QString::number(i) + "Edges", "",
              Qt::ControlModifier + Qt::KeypadModifier + (Qt::Key_1+i),
              [this, i] {
      config::Visualisation::debugDraw.flip(i + visu::Critter::SPLINES_COUNT);
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
  QString title = "Splinoids main window ";
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

void MainView::selectionChanged(visu::Critter *c) {
//  auto q = qDebug();
//  q << "SelectionChanged from "
//    << (selection()? selection()->firstname() : "-1");

  if (selection()) {
    selection()->setSelected(false);
    selection()->object().brainDead = false;
    disconnect(selection(), &visu::Critter::shapeChanged,
               _manipulator, &GeneticManipulator::updateShapeData);
  }

  _simu.setSelection(c);

  if (c) {
    selection()->setSelected(true);
    selection()->object().brainDead =
      config::Visualisation::brainDeadSelection();
    focusOnSelection();

    connect(selection(), &visu::Critter::shapeChanged,
            _manipulator, &GeneticManipulator::updateShapeData);

  } else if (_zoomout)
    showAll();

  _manipulator->setSubject(c);

//  q << "to"
//    << (selection()? selection()->firstname() : "-1");
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
