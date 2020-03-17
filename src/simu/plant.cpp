#include "plant.h"

#include "box2d/b2_circle_shape.h"
#include "box2d/b2_fixture.h"

namespace simu {

Plant::Plant(b2Body *body, float s, float e)
  : _body(*body), _size(s), _energy(e) {

  b2CircleShape cs;
  cs.m_p.Set(0, 0);
  cs.m_radius = .5*_size;

  b2FixtureDef fd;
  fd.shape = &cs;
  fd.density = 1;
  fd.restitution = 0;
  fd.userData = &_color;

  _body.CreateFixture(&fd);

  update();
}

float Plant::maxStorage (float size) {
  return size;
}

void Plant::update(void) {
  _color = {0, .25f * (3+_energy / maxStorage()), 0};
}

} // end of namespace simu
