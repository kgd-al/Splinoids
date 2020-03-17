#include "critter.h"
#include "environment.h"

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

using CubicCoefficients = std::array<real, 4>;
using SplinesCoefficients = std::array<CubicCoefficients,
                                       Critter::SPLINES_PRECISION-2>;

SplinesCoefficients coefficients = [] {
  SplinesCoefficients c;
  for (uint i=0; i<Critter::SPLINES_PRECISION-2; i++) {
    real t = real(i+1) / (Critter::SPLINES_PRECISION-1),
         t_ = 1-t;
    c[i] = {
      real(std::pow(t_, 3)),
      real(3*std::pow(t_, 2)*t),
      real(3*t_*std::pow(t, 2)),
      real(std::pow(t, 3))
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

Critter::Critter(const Genome &g, b2Body *body, float e)
  : _genotype(g), _size(MIN_SIZE), _body(*body), _energy(e) {

  _visionRange = computeVisionRange(_genotype.vision.width);

  _b2Body = nullptr;

  updateShape();
  _motors = { { Motor::LEFT, 0 }, { Motor::RIGHT, 0 } };

  const auto &v = _genotype.vision;
  uint rs_h = 2 * v.precision + 1;
  uint rs = 2 * rs_h;
  _retina.resize(rs, nullptr);
  _raysEnd.resize(rs, P2D(0,0));
  _raysFraction.resize(rs, 1);

  float a0 = v.angleBody;
  _raysStart = {
    bodyRadius() * P2D(std::cos(+a0), std::sin(+a0)),
    bodyRadius() * P2D(std::cos(-a0), std::sin(-a0))
  };

  float r = _visionRange;
  for (uint i=0; i<2; i++) {
    int sgn = (i == 0? 1 : -1);
    for (uint j=0; j<rs_h; j++) {
      float da = 0;
      if (v.precision > 0) da = .5 * v.width * (j / float(v.precision) - 1);
      float a = sgn * (a0 + v.angleRelative + da);
//      std::cerr << "[" << i << "," << j << "] a0 = "
//                << 180*a0/M_PI << "; da = " << 180*da/M_PI
//                << "; a = " << 180*a/M_PI << std::endl;
      _raysEnd[i*rs_h+j] =
        _raysStart[i] + r * P2D(std::cos(a), std::sin(a));

//      std::cerr << "E[" << i*rs_h+j << "] = " << _raysEnd[i*rs_h+j]
//                << " = " << _raysStart[i] << " + "
//                << r * P2D(std::cos(a), std::sin(a)) << std::endl;
    }
    std::cerr << std::endl;
  }
}


void Critter::step(Environment &env) {
//  std::cerr << "Critter " << id() << " at " << pos() << ", angle = "
//            << 180*rotation()/M_PI << std::endl;

  // Launch a bunch of rays
  performVision(env);

  // Apply requested motor output
  for (auto &m: _motors) {
    P2D f = _body.GetWorldVector({config::Simulation::critterBaseSpeed()*m.second,0}),
        p = _body.GetWorldPoint({0, int(m.first)*.5f*bodyRadius()});
    _body.ApplyForce(f, p, true);
  }
}

struct CritterVisionCallback : public b2RayCastCallback {
  float closestFraction;
  b2Fixture *closestContact;
  b2Body *self;

  CritterVisionCallback (b2Body *self) : self(self) { assert(self); }

  void reset (void) {
    closestFraction = 1;
    closestContact = nullptr;
  }

  float ReportFixture(b2Fixture *fixture, const b2Vec2 &/*point*/,
                      const b2Vec2 &/*normal*/, float fraction) override {
//    std::cerr << "Found fixture " << fixture << " at " << fraction
//              << " of type " << fixture->GetBody()->GetType()
//              << std::endl;
    if (self != fixture->GetBody()) {
      closestContact = fixture;
      closestFraction = fraction;
    }
    return fraction;
  }
};

void Critter::performVision(const Environment &env) {
  CritterVisionCallback cvc (&_body);
  uint n = _raysEnd.size(), h = n/2;
  for (uint ie=0; ie<n; ie++) {
    cvc.reset();
    uint is = ie / h;
    env.physics().RayCast(&cvc,
                          _body.GetWorldPoint(_raysStart[is]),
                          _body.GetWorldPoint(_raysEnd[ie]));

    if (cvc.closestContact) { // Found something !
//      std::cerr << "Raycast(" << ie << "): " << cvc.closestFraction
//                << " with " << cvc.closestContact << std::endl;

      _retina[ie] = static_cast<const Color*>(cvc.closestContact->GetUserData());
      _raysFraction[ie] = cvc.closestFraction;

    } else {
      _retina[ie] = nullptr;
      _raysFraction[ie] = 1;
    }
  }
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

  const real r = bodyRadius();
  for (uint i=0; i<SPLINES_COUNT; i++) {
    genotype::Spline::Data &d = _genotype.splines[i].data;
    SplineData &sd = _splinesData[i];
    float w = dimorphism(i);

    P2D p0 = fromPolar(d[S::SA], r); SET(sd.p0, p0)

    sd.al0 = d[S::SA] + W0_RANGE * d[S::W0];
    sd.pl0 = fromPolar(sd.al0, r);

    sd.ar0 = d[S::SA] - W0_RANGE * d[S::W0];
    sd.pr0 = fromPolar(sd.ar0, r);

    sd.p1 = fromPolar(d[S::SA] + d[S::EA], w * (r + d[S::EL]));
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

bool concave (const Critter::Vertices &o, uint &oai) {
  auto n = o.size();
  assert(n == 4);
  for (uint i=0; i<n; i++) {
    P2D pl = o[i<1?n-1:i-1], p = o[i], pr = o[(i+1)%n];
//    std::cerr << /*pl << "-" <<*/ p /*<< "-" << pr*/ << "\t";
    b2Vec2 /*vl = p - pl,*/ vr = pr - p;
//    float a = std::acos(b2Dot(vl, vr) / (vl.Length()*vr.Length()));
//    std::cerr << " has angle(" << vl << ", " << vr << ") = "
//              << a << " (" << 180 * a / M_PI << ") ";
//    if (a > M_PI) std::cerr << " !!!";
//    std::cerr << std::endl;
//    if (a > M_PI) {
//      oai = i;
//      return true;
//    }
    b2Vec2 vl_t (pl.y - p.y, p.x - pl.x);
//    std::cerr << "Dot(" << vl_t << "," << vr << ") = " << b2Dot(vr, vl_t);
//    std::cerr << std::endl;
    if (b2Dot(vr, vl_t) < 0) {
      oai = i;
      return true;
    }
  }
  return false;
}

void Critter::testConvex (const Vertices &o, std::vector<Vertices> &v) {
  assert(o.size() == 4);  // Only works for quads

  P2D itr;
  uint oai;

  // Only need to test outer edges
  if (intersection(o[0], o[1], o[2], o[3], itr)) {
    // Concave quad -> divide in triangles along intersection
    v.push_back({ o[0], itr, o[3] });
    v.push_back({ itr, o[1], o[2] });

  } else if (concave(o, oai)) {
    // Concave quad -> divide along offending vertex
    v.push_back({ o[oai], o[(oai+1)%4], o[(oai+2)%4] });
    v.push_back({ o[oai], o[(oai+2)%4], o[(oai+3)%4] });

  } else  // Convex quad -> keep
    v.push_back(o);
}

b2Fixture* Critter::addBodyFixture (void) {
  b2CircleShape s;
  s.m_p.Set(0, 0);
  s.m_radius = bodyRadius();

  b2FixtureDef fd;
  fd.shape = &s;
  fd.density = BODY_DENSITY;
  fd.friction = .3;
  fd.userData = const_cast<Color*>(&bodyColor());

  return _body.CreateFixture(&fd);
}

b2Fixture* Critter::addPolygonFixture (uint i, const Vertices &v) {
//  float ccs = 0;
//  uint n = v.size();
//  for (uint i=0; i<n; i++)
//    ccs += (v[(i+1)%n].x - v[i].x)*(v[(i+1)%n].y + v[i].y);
//  std::cerr << "Polygon: ";
//  for (auto p: v) std::cerr << " " << p;
//  std::cerr << " (CCScore = " << ccs << ")" << std::endl;

  b2PolygonShape s;
  s.Set(v.data(), v.size());
//  std::cerr << "Valid? " << s.Validate() << std::endl;

//  // Debug
//  bool ok = true;
//  if (s.m_count != v.size())
//    ok = false;
//  else {
//    for (uint j=0; j<v.size(); j++)
//      if (v[j] != s.m_vertices[j])
//        ok = false;
//  }
//  if (!ok) {
//    std::cerr << "Mismatched polygon:\n\t Base";
//    for (uint j=0; j<v.size(); j++) std::cerr << " " << v[j];
//    std::cerr << "\n\tBox2D";
//    for (uint j=0; j<s.m_count; j++) std::cerr << " " << s.m_vertices[j];
//    std::cerr << std::endl;
//  }

//  std::cerr << "Shape: ";
//  for (int j=0; j<s.m_count; j++) std::cerr << " " << s.m_vertices[j];
//  std::cerr << std::endl;

  b2FixtureDef fd;
  fd.shape = &s;
  fd.density = ARTIFACTS_DENSITY;
  fd.friction = 0.;
  fd.userData = const_cast<Color*>(&splineColor(i));

  return _body.CreateFixture(&fd);
}

bool Critter::insideBody(const P2D &p) const {
  return std::sqrt(p.x*p.x+p.y*p.y) <= bodyRadius();
}

bool Critter::internalPolygon(const Vertices &v) const {
  for (const P2D &p: v) if (!insideBody(p)) return false;
  return true;
}

void Critter::updateObjects(void) {
  static constexpr auto P = SPLINES_PRECISION;
  static constexpr auto N = 2*SPLINES_PRECISION-1;

  collisionObjects.clear();

  if (_b2Body)  _body.DestroyFixture(_b2Body);
  _b2Body = addBodyFixture();

  for (uint i=0; i<SPLINES_COUNT; i++) {
    if (dimorphism(i) == 0) continue;
    std::vector<Vertices> objects;

    for (b2Fixture *f: _b2Artifacts[i]) _body.DestroyFixture(f);
    _b2Artifacts[i].clear();

    // Sample spline at requested resolution and split concave quads
    const auto &d = _splinesData[i];
    std::array<P2D, N> p;
    for (uint t=0; t<P; t++)
      p[t] = pointAt(t, d.pl0, d.cl0, d.cl1, d.p1);
    for (uint t=1; t<P; t++)
      p[t+P-1] = pointAt(t, d.p1, d.cr0, d.cr1, d.pr0);

    for (uint k=0; k<P-2; k++)
      testConvex({ p[k], p[k+1], p[N-k-2], p[N-k-1] }, objects);

    // Top triangle
    objects.push_back({ p[P-2], p[P-1], p[P] });

    // Filter out completely internal objects
    for (auto it=objects.begin(); it != objects.end();) {
      if (internalPolygon(*it))
        it = objects.erase(it);
      else
        ++it;
    }

    // Create left components
    uint n = objects.size();
    for (uint i=0; i<n; i++) {
      const Vertices &v = objects[i];
      Vertices v_;
      for (const P2D &p: v) v_.emplace_back(p.x, -p.y);
      objects.push_back(v_);
    }

    // Create b2 object
    for (Vertices &v: objects)
      _b2Artifacts[i].push_back(addPolygonFixture(i, v));

    // Register for debug
    collisionObjects.insert(collisionObjects.end(),
                            objects.begin(), objects.end());
  }
}

void Critter::setMotorOutput(float i, Motor m) {
  assert(-1 <= i && i <= 1);
  assert(EnumUtils<Motor>::isValid(m));
  _motors.at(m) = i;
}

float Critter::efficiency(float age) {
  static constexpr float e = 1e-3, c0 = 1/3., c1 = 2/3.;
  if (age < c0)       return std::tanh(age * std::atanh(1-e)/c0);
  else if (age < c1)  return 1;
  else                return utils::gauss(age, c1, -(1-c1)*(1-c1)/std::log(e));
}

float Critter::computeVisionRange(float visionWidth) {
  return 10/visionWidth;
}

} // end of namespace simu
