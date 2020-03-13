#include <Box2D>

#include "critter.h"

namespace simu {

bool intersection (const P2D &l0, const P2D &l1,
                   const P2D &r0, const P2D &r1,
                   P2D &i) {

  P2D s1 = l1 - l0, s2 = r1 - r0;
  float d = -s2.x * s1.y + s1.x * s2.y;
  if (std::fabs(d) < 1e-6)  return false;

  bool dp = (d > 0);
//  std::cerr << "\nd = " << d << std::endl;

  float t_ = ( s2.x * (l0.y - r0.y) - s2.y * (l0.x - r0.x));
//  std::cerr << "t' = " << t_ << " (" << t_/d << ")" << std::endl;
  if ((t_ < 0) == dp) return false;
  if ((t_ > d) == dp) return false;

  float s_ = (-s1.y * (l0.x - r0.x) + s1.x * (l0.y - r0.y));
//  std::cerr << "s' = " << s_ << " (" << s_/d << ")" << std::endl;
  if ((s_ < 0) == dp) return false;
  if ((s_ > d) == dp) return false;

  float t = t_ / d;
  i = l0 + t * s1;
//  std::cerr << ">> " << i << std::endl;
  return true;
}

struct AABB {
  P2D ll, ur;

  AABB (const P2D &p0, const P2D &p1)
    : ll(std::min(p0.x, p1.x), std::min(p0.y, p1.y)),
      ur(std::max(p0.x, p1.x), std::max(p0.y, p1.y)) {}
};

struct L2D {
  P2D p0, p1;
  AABB aabb;

  L2D (const P2D &p0, const P2D &p1)
    : p0(p0), p1(p1), aabb(p0, p1) {}

  friend bool aabbIntersection (const L2D &lhs, const L2D &rhs) {
    const auto lAABB = lhs.aabb, rAABB = rhs.aabb;
    return lAABB.ll.x <= rAABB.ur.x && rAABB.ll.x <= lAABB.ur.x
        && lAABB.ll.y <= rAABB.ur.y && rAABB.ll.y <= lAABB.ur.y;
  }

  friend bool intersection(const L2D &lhs, const L2D &rhs, P2D &i) {
    return intersection(lhs.p0, lhs.p1, rhs.p0, rhs.p1, i);
  }

  friend bool consecutive (const L2D &lhs, const L2D &rhs) {
    return lhs.p0 == rhs.p0 || lhs.p0 == rhs.p1
        || lhs.p1 == rhs.p0 || lhs.p1 == rhs.p1;
  }

  friend std::ostream& operator<< (std::ostream &os, const L2D &l) {
    return os << "{ " << l.p0 << ", " << l.p1 << " }";
  }
};

using CubicCoefficients = std::array<real, 4>;
using SplinesCoefficients = std::array<CubicCoefficients,
                                       Critter::SPLINES_PRECISION-2>;

SplinesCoefficients coefficients = [] {
  SplinesCoefficients c;
  for (uint i=0; i<Critter::SPLINES_PRECISION-2; i++) {
    real t = real(i+1) / (Critter::SPLINES_PRECISION-1),
         t_ = 1-t;
    c[i] = {
      std::pow(t_, 3), 3*std::pow(t_, 2)*t, 3*t_*std::pow(t, 2), std::pow(t, 3)
    };
  }
  return c;
}();

P2D pointAt (int t, const P2D &p0, const P2D &c0, const P2D &c1, const P2D &p1){
  if (t == 0)
    return p0;
  else if (t == Critter::SPLINES_PRECISION-1)
    return p1;
  else {
    const auto& c = coefficients[t-1];
    return c[0] * p0 + c[1] * c0 + c[2] * c1 + c[3] * p1;
  }
}

Critter::Critter(const Genome &g, float x, float y, float r)
  : _genotype(g), _size(r), _pos(x, y), _rotation(M_PI/2.) {
  updateShape();
}

void Critter::updateShape(void) {
  updateSplines();
  updateObjects();
}

#ifndef NDEBUG
#define SET(LHS, RHS) LHS = RHS;
#else
#define SET(LHS, RHS)
#endif

void Critter::updateSplines(void) {
  using S = genotype::Spline::Index;
  static constexpr auto W0_RANGE = M_PI/2;

  const real r = .5*bodySize();
  for (uint i=0; i<SPLINES_COUNT; i++) {
    genotype::Spline::Data &d = _genotype.splines[i].data;
    SplineData &sd = _splinesData[i];
    float w = dimorphism(i);

    P2D p0 = P2D::fromPolar(d[S::SA], r); SET(sd.p0, p0)

    sd.al0 = d[S::SA] + W0_RANGE * d[S::W0];
    sd.pl0 = P2D::fromPolar(sd.al0, r);

    sd.ar0 = d[S::SA] - W0_RANGE * d[S::W0];
    sd.pr0 = P2D::fromPolar(sd.ar0, r);

    sd.p1 = P2D::fromPolar(d[S::SA] + d[S::EA], w * (r + d[S::EL]));
    P2D v = sd.p1 - p0;
    P2D t (-v.y, v.x);

    P2D pc0, pc1;
    pc0 = p0 + d[S::DX0] * v;  SET(sd.pc0, pc0)
    pc1 = p0 + d[S::DX1] * v;  SET(sd.pc1, pc1)

    P2D c0, c1;
    c0 = p0 + d[S::DX0] * v + d[S::DY0] * t; SET(sd.c0, c0)
    c1 = p0 + d[S::DX1] * v + d[S::DY1] * t; SET(sd.c1, c1)

    sd.cl0 = p0 + d[S::DX0] * v + d[S::DY0] * w * t * (1+d[S::W1]);
    sd.cl1 = p0 + d[S::DX1] * v + d[S::DY1] * w * t * (1+d[S::W2]);

    sd.cr0 = p0 + d[S::DX0] * v - d[S::DY0] * w * t * (1+d[S::W1]);
    sd.cr1 = p0 + d[S::DX1] * v - d[S::DY1] * w * t * (1+d[S::W2]);
  }
}

#undef SET

void Critter::testConvex (const CollisionObject &o) {
  assert(o.size() == 4);  // Only works for quads

  // Only need to test outer edges
  P2D i;
  if (!intersection(o[0], o[1], o[2], o[3], i))
    _collisionObjects.push_back(o); // Convex quad -> keep

  else { // Concave quad -> divide in triangles
    _collisionObjects.push_back({ o[0], o[1], i });
    _collisionObjects.push_back({ i, o[2], o[3] });
  }
}

bool Critter::insideBody(const P2D &p) const {
  return std::sqrt(p.x*p.x+p.y*p.y) <= .5*bodySize();
}

bool Critter::internalPolygon(const CollisionObject &o) const {
  for (const P2D &p: o) if (!insideBody(p)) return false;
  return true;
}

namespace linesweep {

enum Dir : char {   X = 'X', Y = 'Y' };
enum Type : char {  I = 'I', O = 'O' };

struct L2D_CMP {
  bool operator() (const L2D &lhs, const L2D &rhs) {
    return xorder(lhs, rhs);
  }

  static bool xorder (const L2D &lhs, const L2D &rhs) {
    if (lhs.p0.x != rhs.p0.x) return lhs.p0.x < rhs.p0.x;
    if (lhs.p0.x != rhs.p0.x) return lhs.p0.x < rhs.p0.x;
    if (lhs.p0.y != rhs.p0.y) return lhs.p0.y < rhs.p0.y;
    return lhs.p0.y < rhs.p0.y;
  }
};

template <Dir D>
struct Event {
  Type type;
  L2D line;

  real coord (void) const {
    if (type == I)  return coord(line.aabb.ll);
    else            return coord(line.aabb.ur);
  }

  friend bool operator< (const Event &lhs, const Event &rhs) {
    if (lhs.coord() != rhs.coord())     return lhs.coord() < rhs.coord();
    if (lhs.type != rhs.type)           return lhs.type < rhs.type;
    return L2D_CMP::xorder(lhs.line, rhs.line);
  }

  friend std::ostream& operator<< (std::ostream &os, const Event &e) {
    return os << char(D) << char(e.type)
              << " at " << e.coord()
              << " " << e.line;
  }

private:
  static real coord (const P2D &p);
};

using XEvent = Event<X>;
template <> real Event<X>::coord(const P2D &p) {  return p.x; }

using YEvent = Event<Y>;
template <> real Event<Y>::coord(const P2D &p) {  return p.y; }

template <Dir D> const P2D& min (const P2D &lhs, const P2D &rhs);

template <> const P2D& min<X> (const P2D &lhs, const P2D &rhs) {
  return lhs.x < rhs.x ? lhs : rhs;
}

template <> const P2D& min<Y> (const P2D &lhs, const P2D &rhs) {
  return lhs.y < rhs.y ? lhs : rhs;
}

template <Dir D> const P2D& max (const P2D &lhs, const P2D &rhs);

template <> const P2D& max<X> (const P2D &lhs, const P2D &rhs) {
  return lhs.x < rhs.x ? rhs : lhs;
}

template <> const P2D& max<Y> (const P2D &lhs, const P2D &rhs) {
  return lhs.y < rhs.y ? rhs : lhs;
}

} // end of namespace linesweep

// Use variation of Bentley–Ottmann algorithm?
void Critter::mergeAndSplit(void) {
  using namespace linesweep;
  std::set<YEvent> yevents;

  _msIntersections.clear();
  _msLines.clear();

  const auto insertYEvents = [&yevents] (const L2D &l) {
    yevents.insert({I, l});  yevents.insert({O, l});
  };

  std::cerr << _collisionObjects.size() << " objects:\n";
  for (const auto &o: _collisionObjects) {
    for (uint i=0; i<o.size()-1; i++) insertYEvents({o[i], o[i+1]});
    insertYEvents({o.back(), o.front()});

    std::cerr << "\t";
    for (const auto &p: o)  std::cerr << p << " ";
    std::cerr << "\n";
  }
  std::cerr << std::endl;

  std::set<L2D, L2D_CMP> currentLines;
  P2D lastI;

  std::cerr << yevents.size() << " YEvents:\n";
  for (const YEvent &e: yevents)
    std::cerr << "\t" << e << "\n";
  std::cerr << std::endl;

//  std::set<XEvent> xevents;
  while (!yevents.empty()) {
    auto it = yevents.begin();
    YEvent ye = *it;
    yevents.erase(it);

    std::cerr << "Processing " << ye << "\n";

//    std::initializer_list<XEvent> events {
//      { I, ye.line }, { O, ye.line }
//    };
    if (ye.type == I) {
      for (const L2D &l: currentLines)
        std::cerr << "\t" << l << "\n";

      bool foundI = false;
      for (const L2D &l: currentLines) {
        if (!consecutive(ye.line, l)
            && aabbIntersection(ye.line, l)
            && intersection(ye.line, l, lastI)) {

          std::cerr << "Intersection "
                    << " at " << lastI
                    << " between " << ye.line << " and " << l
                    << std::endl;

          foundI = true;
          _msIntersections.push_back(lastI);
          L2D lm { min<Y>(ye.line.p0, ye.line.p1), lastI },
              lM { lastI, max<Y>(ye.line.p0, ye.line.p1) },
              rm { min<Y>(l.p0, l.p1), lastI },
              rM { lastI, max<Y>(l.p0, l.p1) };

          _msLines.push_back({lm.p0, lm.p1});
          _msLines.push_back({rm.p0, rm.p1});

          yevents.erase({ O, ye.line });
          yevents.erase({ O, l });

          yevents.insert({ O, lM });  currentLines.insert(lM);
          yevents.insert({ O, rM });  currentLines.insert(rM);
        }
      }

      if (!foundI) {
        auto n = currentLines.size();
        currentLines.insert(ye.line);
        assert(n < currentLines.size());
  //      xevents.insert(events);
      }

    } else {
      currentLines.erase(ye.line);
//      for (auto &e: events) xevents.erase(e);
    }
  }

  std::cerr << "Merge/split completed with " << _msIntersections.size()
            << " intersections" << std::endl;
}

void Critter::updateObjects(void) {
  static constexpr auto P = SPLINES_PRECISION;
  static constexpr auto N = 2*SPLINES_PRECISION-1;

  _collisionObjects.clear();
  for (uint i=0; i<SPLINES_COUNT; i++) {

    if (dimorphism(i) > 0) {
      const auto &d = _splinesData[i];
      std::array<P2D, N> p;
      for (uint t=0; t<P; t++)
        p[t] = pointAt(t, d.pl0, d.cl0, d.cl1, d.p1);
      for (uint t=1; t<P; t++)
        p[t+P-1] = pointAt(t, d.p1, d.cr0, d.cr1, d.pr0);

      for (uint k=0; k<P-2; k++)
        testConvex({ p[k], p[k+1], p[N-k-2], p[N-k-1] });

      // Top triangle
      _collisionObjects.push_back({
        p[P-2], p[P-1], p[P]
      });
    }
  }

  uint n = _collisionObjects.size();
  for (uint i=0; i<n; i++) {
    const CollisionObject &o = _collisionObjects[i];
    CollisionObject o_;
    for (const P2D &p: o) o_.emplace_back(p.x, -p.y);
    _collisionObjects.push_back(o_);
  }
  uint n2 = _collisionObjects.size();

  for (auto it=_collisionObjects.begin(); it != _collisionObjects.end();) {
    if (internalPolygon(*it))
      it = _collisionObjects.erase(it);
    else
      ++it;
  }
  uint n3 = _collisionObjects.size();

//  if (!_collisionObjects.empty()) mergeAndSplit();

//  uint n_ = _collisionObjects.size();
//  std::cerr << n_ << " objects";
//  if (n_ != n2) std::cerr << ", " << n2 << " before filtering";
//  if (n_ != n3) std::cerr << ", " << n2 << " before merging";
//  std::cerr << std::endl;

//  std::cerr << "Objects:\n";
//  uint i=0;
//  for (const auto &o: _collisionObjects) {
//    std::cerr << "\t" << i++ << ":";
//    for (const auto &p: o)
//      std::cerr << " (" << p.x << ", " << p.y << ")";
//    std::cerr << "\n";
//  }
//  std::cerr << std::endl;
}

} // end of namespace simu
