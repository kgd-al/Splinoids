#ifndef SIMU_FOODLET_H
#define SIMU_FOODLET_H

#include "config.h"
#include "box2d/b2_body.h"

namespace simu {

struct Environment;

class Foodlet {
  BodyType _type;
  uint _id;
  b2Body &_body;
  float _radius;
  decimal _energy, _maxEnergy;
  config::Color _color;
  config::Color _baseColor;
  b2BodyUserData _userData;

public:
  Foodlet(BodyType type, uint id, b2Body *body, float radius, decimal energy);

  auto type (void) const {
    return _type;
  }

  bool isPlant (void) const {
    return _type == BodyType::PLANT;
  }

  bool isCorpse (void) const {
    return _type == BodyType::CORPSE;
  }

  auto id (void) const {
    return _id;
  }

  auto& body (void) {
    return _body;
  }

  float radius (void) const {
    return _radius;
  }

  static decimal maxStorage(BodyType type, float radius);

  decimal energy (void) const {
    return _energy;
  }

  void consumed (decimal e);

  decimal fullness (void) const {
    return _energy / _maxEnergy;
  }

  const auto& color (void) const {
    return _color;
  }

  void setBaseColor (config::Color base) {
    _baseColor = base;
  }

  void update (Environment &env);
  void updateColor (void);
};

} // end of namespace simu

#endif // SIMU_FOODLET_H
