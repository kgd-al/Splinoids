#ifndef SIMU_CRITTER_H
#define SIMU_CRITTER_H

#include "../genotype/critter.h"
#include "config.h"

#include "box2d/b2_body.h"

DEFINE_PRETTY_ENUMERATION(Motor, LEFT = -1, RIGHT = 1)

namespace simu {

struct Environment;

class Critter {
public:
  using Genome = genotype::Critter;
  using Sex = Genome::Sex;
  using ID = phylogeny::GID;

  static constexpr float MIN_SIZE = 1;
  static constexpr float MAX_SIZE = 1;

  static constexpr float BODY_DENSITY = 1;
  static constexpr float ARTIFACTS_DENSITY = 2;

  static constexpr auto SPLINES_COUNT = Genome::SPLINES_COUNT;
  static constexpr uint SPLINES_PRECISION = 4;

private:
  Genome _genotype;

  float _visionRange; // Derived from genotype
  float _size; // Ratio

//  uint foo;

  b2Body &_body;

  struct SplineData {
    // Central spline
#ifndef NDEBUG
    P2D p0;   // Origin
    P2D c0;   // First control point
    P2D c1;   // Second control point
#endif
    P2D p1;   // Destination

#ifndef NDEBUG
    // Control points projection on (p1-p0)
    P2D pc0;  // First
    P2D pc1;  // Second
#endif

    // Angular offsets
    float al0;  // Left spline
    float ar0;  // Right spline

    // Left spline
    P2D pl0;  // Origin
    P2D cl0;  // First control point
    P2D cl1;  // Second control point

    // Right spline
    P2D pr0;  // Destination
    P2D cr0;  // First control point
    P2D cr1;  // Second control point
  };
  std::array<SplineData, SPLINES_COUNT> _splinesData;

  b2Fixture *_b2Body;
  std::array<std::vector<b2Fixture*>, SPLINES_COUNT> _b2Artifacts;

  std::map<Motor, float> _motors;

  // Vision cache data (each vector of size 2*(2*genotype.vision.precision+1))
  std::vector<const Color*> _retina;
  std::array<P2D, 2> _raysStart;
  std::vector<P2D> _raysEnd;
  std::vector<float> _raysFraction; // TODO Remove (debug visu only)

  float _energy, _health, _water;

public:
  // TODO Remove these variables
  using Vertices = std::vector<P2D>;
  std::vector<Vertices> collisionObjects;

  Critter(const Genome &g, b2Body *body, float e);

  void step (Environment &env);

  const auto& genotype (void) const {
    return _genotype;
  }

  auto& genotype (void) {
    return _genotype;
  }

  auto id (void) const {
    return genotype().id();
  }

  auto sex (void) const {
    return genotype().sex();
  }

  // Dimorphism coefficient for current sex and specified spline
  auto dimorphism (uint i) const {
    return _genotype.dimorphism[4*sex()+i];
  }

  auto pos (void) const {
    return _body.GetPosition();
  }

  auto x (void) const {
    return pos().x;
  }

  auto y (void) const {
    return pos().y;
  }

  // in radians
  auto rotation (void) const {
    return _body.GetAngle();
  }

  const b2Body& body (void) const {
    return _body;
  }

  b2Body& body (void) {
    return _body;
  }

  /*
   * In ]0,1]. Ratio of maximal size
   */
  auto bodySize (void) const {
    return _size * MAX_SIZE;
  }

  /*
   * In ]0,1]. Ratio of maximal size
   */
  auto bodyRadius (void) const {
    return .5f * bodySize();
  }

  /*
   * In ]0,2]. (Potential) Size of body + artifacts
   */
//  auto size (void) const {
//    return 2 * bodySize();
//  }

  const auto& splinesData (void) const {
    return _splinesData;
  }

  const auto fixturesList (void) const {
    return _body.GetFixtureList();
  }

  float visionRange (void) const {
    return _visionRange;
  }

  const auto& raysStart (void) const {
    return _raysStart;
  }

  const auto& retina (void) const {
    return _retina;
  }

  const auto& raysEnd (void) const {
    return _raysEnd;
  }

  const auto &raysLength (void) const {
    return _raysFraction;
  }

  auto mass (void) const {
    return _body.GetMass();
  }

  void updateShape (void);

  void setMotorOutput (float i, Motor m);

  float motorOutput (Motor m) const {
    return _motors.at(m);
  }

  const Color& bodyColor (void) const {
    return _genotype.colors[5*sex()];
  }

  const Color& splineColor (uint i) const {
    return _genotype.colors[5*sex()+i];
  }

  // age in [0,1]
  static float efficiency (float age);

  // MIN_SIZE <= size <= MAX_SIZE
  static float storage (float size) {
    return M_PI * .25 * size * size;
  }

  static float computeVisionRange (float visionWidth);

private:
  void performVision (const Environment &env);

  void updateSplines (void);
  void updateObjects (void);

  static void testConvex (const Vertices &o, std::vector<Vertices> &v);
  bool internalPolygon (const Vertices &v) const;

  bool insideBody (const P2D &p) const;

  b2Fixture* addBodyFixture (void);
  b2Fixture* addPolygonFixture(uint i, const Vertices &v);
};

} // end of namespace simu

#endif // simu_CRITTER_H
