#include "joystick.h"
#include <cmath>

namespace gui {

void PersitentJoystick::update (void) {
  JoystickEvent event;
  while (low_level_object.sample(&event)) {
    if (event.type == JS_EVENT_INIT)  continue;

//    std::cerr << "Event: " << event << std::endl;

    mappings[event.type][event.number] = event.value;

    auto b = MyOwnControllerMapping(event.number);
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
