#include "foodlet.h"
#include "environment.h"

#include "box2d/b2_circle_shape.h"
#include "box2d/b2_fixture.h"

namespace simu {

Foodlet::Foodlet(BodyType type, uint id, b2Body *body, float r, float e)
  : _type(type), _id(id), _body(*body), _radius(r), _energy(e) {

  assert(_type == BodyType::PLANT || _type == BodyType::CORPSE);

  b2CircleShape cs;
  cs.m_p.Set(0, 0);
  cs.m_radius = _radius;

  b2FixtureDef fd;
  fd.shape = &cs;
  fd.density = 1;
  fd.restitution = 0;
  fd.userData = &_color;

  _body.CreateFixture(&fd);

  _userData.type = BodyType::PLANT;
  _userData.ptr.foodlet = this;
  _body.SetUserData(&_userData);

  _maxEnergy = maxStorage(_type, _radius);
  assert(_energy <= _maxEnergy);

  _baseColor = utils::uniformStdArray<Color>(0);
  updateColor();
}

float Foodlet::maxStorage (BodyType type, float radius) {
  static const float ped = config::Simulation::plantEnergyDensity();
  static const float her = config::Simulation::healthToEnergyRatio();

  float s = M_PI * radius * radius;
  if (type == BodyType::PLANT)  s *= ped;
  else if (type == BodyType::CORPSE)  s *= 1+her;
  return s;
}

void Foodlet::consumed(float de) {
  assert(0 <= de && de <= _energy);
  _energy -= de;
  updateColor();
}

void Foodlet::updateColor(void) {
  float f = fullness();
  if (_type == BodyType::PLANT)
    _color = {0, .25f*(3+f), 0};

  else if (_type == BodyType::CORPSE)
    _color = {f*_baseColor[0], f*_baseColor[1], f*_baseColor[2]};
}

void Foodlet::update(Environment &env) {
  if (_type == BodyType::CORPSE) {
    float de = config::Simulation::decompositionRate() * env.dt();
    _energy -= de;
    env.modifyEnergyReserve(+de);
  }
  updateColor();
}

} // end of namespace simu
