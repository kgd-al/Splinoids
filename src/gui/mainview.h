#ifndef MAINVIEW_H
#define MAINVIEW_H

#include <QGraphicsView>
#include <QMenuBar>
#include <QTimer>

#include "../visu/graphicsimulation.h"

#include "statsview.h"
#include "joystick.h"

namespace gui {

struct GeneticManipulator;

class MainView : public QGraphicsView {
  QTimer _stepTimer;

  visu::GraphicSimulation &_simu;
  GeneticManipulator *_manipulator;

  StatsView *_stats;

  QMenuBar *_mbar;
  QMap<QString, QAction*> _actions;

  PersitentJoystick _joystick;
  using JButton = PersitentJoystick::MyOwnControllerMapping;

  bool _running, _stepping;
  bool _zoomout;

public:
  MainView (visu::GraphicSimulation &simulation, StatsView *stats,
            QMenuBar *bar);

  void joystickEvent (JButton b, PersitentJoystick::Value v);

  void keyReleaseEvent(QKeyEvent *e) override;

  void mouseReleaseEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;

  void resizeEvent(QResizeEvent *e) override;

  void selectNext (void);
  void selectPrevious (void);

  void start (uint speed = 1);
  void stop (void);
  void toggle (void);

  void stepping (bool s);

  void step (void);

  void toggleCharacterSheet (void);

  void showAll();

private:
  void buildActions (void);

  template <typename F>
  void addBoolAction (QMenu *m, const QString &iname, const QString &name,
                      const QString &details, QKeySequence k, F f, bool &v);

  template <typename F, typename T>
  void addEnumAction (QMenu *m, const QString &iname, const QString &name,
                      const QString &details, QKeySequence k, F f,
                      T &v, T max);

  template <typename T> using Inc = std::function<T(T)>;
  template <typename T> using Fmt =
    std::function<std::ostream&(std::ostream&, const T&)>;

  template <typename F, typename T>
  void addEnumAction (QMenu *m, const QString &iname, const QString &name,
                      const QString &details, QKeySequence k, F f,
                      T &v, const Inc<T> &next, const Fmt<T> &format =
      Fmt<T>([] (std::ostream &os, const T &v) -> std::ostream& {
        return os << v;
      }));

  template <typename F>
  void addAction (QMenu *m, const QString &iname, const QString &name,
                  const QString &details, QKeySequence k, F f);

  template <typename F>
  void addAction (QMenu *m, QAction *a, const QString &iname,
                  const QString &details, QKeySequence k, F f);

  void updateWindowName(void);

  void selectionChanged (visu::Critter *c);
  void focusOnSelection (void);

  void externalCritterControl (void);

  visu::Critter* selection (void) {
    return _simu.selection();
  }

  void setSelection (visu::Critter *c);
};

} // end of namespace gui

#endif // MAINVIEW_H
