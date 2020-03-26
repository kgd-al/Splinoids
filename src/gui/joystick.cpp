#include "joystick.h"
#include <cmath>

namespace gui {

void PersitentJoystick::update (void) {
  using M = MyOwnControllerMapping;

  JoystickEvent event;
  while (low_level_object.sample(&event)) {
    if (event.type == JS_EVENT_INIT)  continue;

//    std::cerr << "Event: " << event << std::endl;

    mappings[event.type][event.number] = event.value;

    auto b = M(event.number);

    // Consider d-pad as both button and axis
    if (event.type == JS_EVENT_AXIS) {
      if (b == M::AXIS_DPAD_X)
        b = event.value < 0 ? M::DPAD_LEFT : M::DPAD_RIGHT;
      else if (b == M::AXIS_DPAD_Y)
        b = event.value < 0 ? M::DPAD_DOWN : M::DPAD_UP;
      if (b != M(event.number)) {
        event.type = JS_EVENT_BUTTON;
        event.value = (event.value != 0);
      }
    }

    if (event.type == JS_EVENT_BUTTON
        && signalMapped.find(b) != signalMapped.end())
      emit buttonChanged(b, event.value);
  }
}

float PersitentJoystick::axisValue (MyOwnControllerMapping c) {
  return value(JS_EVENT_AXIS, Control(c));
}

float PersitentJoystick::buttonValue (MyOwnControllerMapping c) {
  return value(JS_EVENT_BUTTON, Control(c));
}

float PersitentJoystick::value (Type t, Control c) {
  auto it = mappings.find(t);
  if (it == mappings.end()) return NAN;

  auto it2 = it->second.find(c);
  if (it2 == it->second.end()) return NAN;

  float v = it2->second;
  if (t == JS_EVENT_AXIS) v /= JoystickEvent::MAX_AXES_VALUE;

  return v;
}

} // end of namespace gui
