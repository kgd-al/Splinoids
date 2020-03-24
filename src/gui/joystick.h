#ifndef GUI_JOYSTICK_H
#define GUI_JOYSTICK_H

#include <set>

#include <QObject>

#include "../../joystick/joystick.hh"

namespace gui {

class PersitentJoystick : public QObject {
  Q_OBJECT
public:
  using Type = decltype(JoystickEvent::type);
  using Control = decltype(JoystickEvent::number);
  using Value = decltype(JoystickEvent::value);

  enum MyOwnControllerMapping : Control { // Which won't work for you
    AXIS_L_X = 0,
    AXIS_L_Y = 1,

    AXIS_L_T = 2,

    AXIS_R_X = 3,
    AXIS_R_Y = 4,

    AXIS_R_T = 5,


    A = 0,
    B = 1,
    X = 2,
    Y = 3,

    LB = 4,
    RB = 5,

    SELECT = 6,
    START = 7
  };

private:
  Joystick low_level_object;

  std::map<Type, std::map<Control, Value>> mappings;

  std::set<MyOwnControllerMapping> signalMapped;

public:

  void setSignalManagedButtons (const std::set<MyOwnControllerMapping> &l) {
    signalMapped = l;
  }

  void update (void);

  float axisValue (MyOwnControllerMapping c);
  float buttonValue (MyOwnControllerMapping c);

signals:

  void buttonChanged (MyOwnControllerMapping c, Value v);

private:
  float value (Type t, Control c);
};

} // end of namespace gui

#endif // GUI_JOYSTICK_H
