#include "foodlet.h"
#include "environment.h"
#include "box2dutils.h"

#include "box2d/b2_circle_shape.h"
#include "box2d/b2_fixture.h"

namespace simu {

Foodlet::Foodlet (uint id, b2Body *body) : _id(id), _body(*body) {}

Foodlet::Foodlet(BodyType type, uint id, b2Body *body, float r, decimal e)
  : _id(id), _body(*body), _radius(r), _energy(e) {

  assert(type == BodyType::PLANT || type == BodyType::CORPSE);
  _userData.type = type;
  _userData.ptr.foodlet = this;

  b2CircleShape cs;
  cs.m_p.Set(0, 0);
  cs.m_radius = _radius;

  b2FixtureDef fd;
  fd.shape = &cs;
  fd.density = 1;
  fd.restitution = 0;
  fd.userData = &_color;

  if (isPlant()) {
    fd.filter.categoryBits = uint16(CollisionFlag::PLANT_FLAG);
    fd.filter.maskBits = uint16(CollisionFlag::PLANT_MASK);

  } else if (isCorpse()) {
    fd.filter.categoryBits = uint16(CollisionFlag::CORPSE_FLAG);
    fd.filter.maskBits = uint16(CollisionFlag::CORPSE_MASK);
  }

  _body.CreateFixture(&fd);

  _body.SetUserData(&_userData);

  _maxEnergy = maxStorage(type, _radius);
  assert(_energy <= _maxEnergy);

  _baseColor = {0,1,0};
  updateColor();
}

decimal Foodlet::maxStorage (BodyType type, float radius) {
  static const float ped = config::Simulation::plantEnergyDensity();
  static const decimal her = config::Simulation::healthToEnergyRatio();

  float s = M_PI * radius * radius;
  if (type == BodyType::PLANT)  s *= ped;
  else if (type == BodyType::CORPSE)  s *= 1+her;
  return s;
}

void Foodlet::consumed(decimal de) {
  assert(0 <= de && de <= _energy);
  _energy -= de;
  updateColor();
}

void Foodlet::setBaseColor(config::Color base) {
  _baseColor = base;
  updateColor();
}

void Foodlet::updateColor(void) {
  float f = float(fullness());
  if (isPlant())  f = .25f*(3+f);
  _color = {f*_baseColor[0], f*_baseColor[1], f*_baseColor[2]};
}

void Foodlet::update(Environment &env) {
  if (isCorpse()) {
    decimal de = config::Simulation::decompositionRate() * env.dt();
    utils::iclip_max(de, _energy);
    _energy -= de;
    env.modifyEnergyReserve(+de);
  }
  updateColor();
}

Foodlet* Foodlet::clone (const Foodlet *f, b2Body *b) {
  Foodlet *this_f = new Foodlet(f->id(), b);

#define COPY(X) this_f->X = f->X;
  COPY(_radius);
  COPY(_energy);
  COPY(_maxEnergy);
  COPY(_color);
  COPY(_baseColor);
  COPY(_userData);
#undef COPY

  b2Fixture *fxt = f->_body.GetFixtureList();
  b2Fixture *this_fxt = Box2DUtils::clone(fxt, b);
  this_fxt->SetUserData(&this_f->_color);

  this_f->_userData.ptr.foodlet = this_f;
  this_f->_body.SetUserData(&this_f->_userData);

  assert(&f->_userData == f->_body.GetUserData());
  assert(&this_f->_userData == b->GetUserData());
  assert(f->_body.GetUserData() != this_f->_body.GetUserData());

  return this_f;
}

void assertEqual (const Foodlet &lhs, const Foodlet &rhs, bool deepcopy) {
  using utils::assertEqual;
#define ASRT(X) assertEqual(lhs.X, rhs.X, deepcopy)
  ASRT(_id);
  ASRT(_body);
  ASRT(_radius);
  ASRT(_energy);
  ASRT(_maxEnergy);
  ASRT(_color);
  ASRT(_baseColor);
  ASRT(_userData);
#undef ASRT
}

nlohmann::json Foodlet::save (const Foodlet &f) {
  return nlohmann::json {
    f._userData.type, f._id, f._radius, f._energy, f._baseColor
  };
}

Foodlet* Foodlet::load (const nlohmann::json &j, b2Body *body) {
  Foodlet *f = new Foodlet(j[0], j[1], body, j[2], j[3]);
  f->setBaseColor(j[4]);
  return f;
}

} // end of namespace simu
