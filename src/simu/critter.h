#ifndef SIMU_CRITTER_H
#define SIMU_CRITTER_H

#include "../genotype/critter.h"

namespace simu {

using real = double;
struct P2D {
  real x, y;

  P2D (void) : P2D(0,0) {}

  P2D (real x, real y) : x(x), y(y) {}

  P2D operator+ (const P2D &that) const {
    return P2D (this->x + that.x, this->y + that.y);
  }

  P2D operator- (const P2D &that) const {
    return P2D (this->x - that.x, this->y - that.y);
  }

  P2D operator* (real v) const {
    return P2D(this->x * v, this->y * v);
  }

  friend P2D operator* (real v, const P2D &p) {
    return p * v;
  }

  friend bool operator== (const P2D &lhs, const P2D &rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y;
  }

  friend std::ostream& operator<< (std::ostream &os, const P2D &p) {
    return os << "{ " << p.x << ", " << p.y << " }";
  }

  template <typename T,
            typename V = decltype(std::declval<T>().x()),
            typename std::enable_if<std::is_same_v<V, real>, int>::type = 0>
  operator T(void) const {
    return T(x, y);
  }

  static P2D fromPolar (float a, float l) {
    return l * P2D(std::cos(a), std::sin(a));
  }
};

class Critter {
public:
  using Genome = genotype::Critter;
  using Sex = Genome::Sex;
  using ID = phylogeny::GID;

  static constexpr float MAX_SIZE = 1;
  static constexpr auto SPLINES_COUNT = Genome::SPLINES_COUNT;
  static constexpr uint SPLINES_PRECISION = 4;

private:
  Genome _genotype;

  float _size; // Ratio

  P2D _pos;

  float _rotation;

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

  using CollisionObject = std::vector<P2D>;
  using CollisionObjects = std::vector<CollisionObject>;
  CollisionObjects _collisionObjects;

public:
  std::vector<P2D> _msIntersections;
  std::vector<std::array<P2D,2>> _msLines;

  Critter(const Genome &g, float x, float y, float r);

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
    return _pos;
  }

  auto x (void) const {
    return _pos.x;
  }

  auto y (void) const {
    return _pos.y;
  }

  // in radians
  auto rotation (void) const {
    return _rotation;
  }

  /*
   * In ]0,1]. Ratio of maximal size
   */
  auto bodySize (void) const {
    return _size * MAX_SIZE;
  }

  /*
   * In ]0,2]. (Potential) Size of body + artifacts
   */
  auto size (void) const {
    return 2 * bodySize();
  }

  const auto& splinesData (void) const {
    return _splinesData;
  }

  const auto& collisionObjects (void) const {
    return _collisionObjects;
  }

  void updateShape (void);

private:
  void updateSplines (void);
  void updateObjects (void);

  void testConvex (const CollisionObject &o);
  bool internalPolygon (const CollisionObject &o) const;

  bool insideBody (const P2D &p) const;

  void mergeAndSplit (void);
};

} // end of namespace simu

#endif // simu_CRITTER_H
