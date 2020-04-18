#ifndef SIMU_FOODLET_H
#define SIMU_FOODLET_H

#include "config.h"
#include "box2d/b2_body.h"

namespace simu {

struct Environment;

class Foodlet {
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
    return _userData.type;
  }

  bool isPlant (void) const {
    return type() == BodyType::PLANT;
  }

  bool isCorpse (void) const {
    return type() == BodyType::CORPSE;
  }

  auto id (void) const {
    return _id;
  }

  auto& body (void) {
    return _body;
  }

  const auto& pos (void) const {
    return _body.GetPosition();
  }

  auto x (void) const {
    return pos().x;
  }

  auto y (void) const {
    return pos().y;
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

  static nlohmann::json save (const Foodlet &f);
  static Foodlet* load (const nlohmann::json &j, b2Body *body);
};

} // end of namespace simu

#endif // SIMU_FOODLET_H
