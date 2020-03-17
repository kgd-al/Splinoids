#ifndef PLANT_H
#define PLANT_H

#include "../genotype/config.h"
#include "box2d/b2_body.h"

namespace simu {

class Plant {
  b2Body &_body;
  float _size;
  float _energy;
  config::Color _color;

public:
  Plant(b2Body *body, float size, float energy);

  auto& body (void) {
    return _body;
  }

  float size (void) const {
    return _size;
  }

  static float maxStorage (float size);
  float maxStorage (void) const {
    return maxStorage(_size);
  }

  float energy (void) const {
    return _energy;
  }

  float consume (float e);

  float fullness (void) const {
    return _energy / maxStorage();
  }

  const auto& color (void) const {
    return _color;
  }

  void update (void);
};

} // end of namespace simu

#endif // PLANT_H
