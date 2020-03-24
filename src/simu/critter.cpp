#include "critter.h"
#include "environment.h"

namespace simu {

Critter::FixtureData::FixtureData (FixtureType t, const Color &c,
                                   uint si, Side fs, uint ai)
  : type(t), color(c), sindex(si), sside(fs), aindex(ai), centerOfMass(P2D()) {}

std::ostream& operator<< (std::ostream &os, const Critter::Side &s) {
  return os << (s == Critter::Side::LEFT ? "L" : "R");
}

std::ostream& operator<< (std::ostream &os, const Critter::FixtureData &fd) {
  if (fd.type == Critter::FixtureType::BODY)
    return os << "B";
  else
    return os << fd.sindex << fd.sside << fd.aindex;
}

Critter::FixtureData* Critter::get (b2Fixture *f) {
  return static_cast<FixtureData*>(f->GetUserData());
}

b2BodyUserData* Critter::get (b2Body *b) {
  return static_cast<b2BodyUserData*>(b->GetUserData());
}

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

P2D ySymmetrical (const P2D &p) {
  return {p.x,-p.y};
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
  : _genotype(g), _size(MIN_SIZE), _body(*body) {

  static const float initEnergyRatio =
    1 + config::Simulation::healthToEnergyRatio();

  _visionRange = computeVisionRange(_genotype.vision.width);

  _b2Body = nullptr;

  // Debug
  _masses.fill(0);
  _currHealth.fill(0);

  updateShape();  

  _motors = { { Motor::LEFT, 0 }, { Motor::RIGHT, 0 } };
  clockSpeed(.5);
  assert(0 <= _clockSpeed && _clockSpeed < 10);

  _age = 0;
  _energy = e / initEnergyRatio;
//  _maxEnergy = initialEnergyStorage();

  // Update current healths
  _currHealth[0] = e * (1 - 1/initEnergyRatio) / config::Simulation::healthToEnergyRatio();
  for (uint i=0; i<SPLINES_COUNT; i++)
    for (Side s: {Side::LEFT, Side::RIGHT})
      _currHealth[1+splineIndex(i, s)] = splineMaxHealth(i, s);

  _destroyed.reset(0);

//  std::cerr << "Received " << e << " energy\n"
//            << bodyHealth() << " = " << bodyMaxHealth() << " * (1 - " << e
//              << " / " << initEnergyRatio << ")\n"
//            << "C" << id() << " energy breakdown:\n"
//            << "\tReserve " << _energy << " / " << maxUsableEnergy() << "\n"
//            << "\t Health " << bodyHealth()
//              << " / " << bodyMaxHealth() << "\n"
//            << "\t  Total " << totalEnergy() << std::endl;

  utils::iclip_max(_energy, _masses[0]);  // WARNING Loss of precision
  assert(_energy <= _masses[0]);
  assert(bodyHealth() <= bodyMaxHealth());

//  std::cerr << "Splinoid " << id() << "\n";
//  std::cerr << "     Body health: " << bodyHealth() << " / " << bodyMaxHealth()
//            << "\n";

//  for (Side s: {Side::LEFT, Side::RIGHT})
//    for (uint i=0; i<SPLINES_COUNT; i++)
//      std::cerr << "Spline " << (s == Side::LEFT ? "L" : "R") << i
//                << " health: " << splineHealth(i, s) << " / "
//                << splineMaxHealth(i) << "\n";
//  std::cerr << std::endl;

  const auto &v = _genotype.vision;
  uint rs_h = 2 * v.precision + 1;
  uint rs = 2 * rs_h;
  _retina.resize(rs, Color());
  _raysEnd.resize(rs, P2D(0,0));
  _raysFraction.resize(rs, 1);

  float a0 = v.angleBody;
  _raysStart = {
    bodyRadius() * P2D(std::cos(+a0), std::sin(+a0)),
    bodyRadius() * P2D(std::cos(-a0), std::sin(-a0))
  };

  float r = _visionRange;
  for (uint i=0; i<rs_h; i++) {
    float da = 0;
    if (v.precision > 0) da = .5 * v.width * (i / float(v.precision) - 1);
    float a = a0 + v.angleRelative + da;
//    std::cerr << "[" << i << "] a0 = "
//              << 180*a0/M_PI << "; da = " << 180*da/M_PI
//              << "; a = " << 180*a/M_PI << std::endl;

    uint il = rs_h-i-1, ir = rs_h+i;
    _raysEnd[il] = _raysStart[0] + r * P2D(std::cos(a), std::sin(a));
    _raysEnd[ir] = _raysEnd[il];
    _raysEnd[ir].y *= -1;

//    std::cerr << "E[" << il << "] = " << _raysEnd[il]
//              << " = " << _raysStart[0] << " + "
//              << r * P2D(std::cos(a), std::sin(a)) << std::endl;
//    std::cerr << "E[" << ir << "] = " << _raysEnd[ir]
//                 << " = { " << _raysEnd[il].x << ", " << -_raysEnd[il].y
//                 << " }" << std::endl;
//    std::cerr << std::endl;
  }
//  std::cerr << std::endl;

  _objectUserData.type = BodyType::CRITTER;
  _objectUserData.ptr.critter = this;
  _body.SetUserData(&_objectUserData);
}


void Critter::step(Environment &env) {
  // Launch a bunch of rays
  performVision(env);

  // Query neural network
  neuralStep();

  // Distribute energy
  energyConsumption(env);

//  if (id() == ID(2) && _age >.01)  _age = 1;

  // Age-specific tasks
  _age += _clockSpeed * env.dt() * config::Simulation::baselineAgingSpeed();
  if (_age < 1/3.)
    growthStep();
  else if (_age < 2/3.)
    reproduction();
  else
    senescence();

  // TODO Smell?
}

void Critter::autopsy (void) const {
  if (!isDead())  std::cerr << "Please don't\n";
  else {
    std::cerr << "Critter " << id() << " died of";
    if (tooOld())   std::cerr << " old age";
    if (starved())  std::cerr << " starvation";
    if (fatallyInjured()) std::cerr << " injuries";
    std::cerr << "\n";
  }
}

// =============================================================================
// == Substeps

#define ERR \
  std::cerr << __PRETTY_FUNCTION__ << " not implemented" << std::endl;
#define ERR // TODO

void Critter::performVision(const Environment &env) {
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
        return fraction;

      } else
        return -1;
    }
  } cvc (&_body);

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

      b2Body *other = cvc.closestContact->GetBody();
      switch (get(other)->type) {
      case BodyType::CRITTER:
        _retina[ie] = get(cvc.closestContact)->color;
        break;
      case BodyType::PLANT:
      case BodyType::CORPSE:
        _retina[ie] = *static_cast<const Color*>(
                        cvc.closestContact->GetUserData());
        break;

      case BodyType::OBSTACLE:
        _retina[ie] = utils::uniformStdArray<Color>(1);
        break;

      default:
        throw std::logic_error("Invalid body type");
      }

      _raysFraction[ie] = cvc.closestFraction;

    } else {
      _retina[ie] = Color();
      _raysFraction[ie] = 1;
    }
  }
}

void Critter::neuralStep(void) {  ERR
  // Set inputs
  // Process n propagation steps
  // Collect outputs

  // Apply requested motor output
  for (auto &m: _motors) {
    float s = config::Simulation::critterBaseSpeed() * m.second * _clockSpeed;
    P2D f = _body.GetWorldVector({s,0}),
        p = _body.GetWorldPoint({0, int(m.first)*.5f*bodyRadius()});
    _body.ApplyForce(f, p, true);
  }
}

void Critter::energyConsumption (Environment &env) {  ERR
  float dt = env.dt();
  float de = 0;

  de += config::Simulation::baselineEnergyConsumption();
  de += config::Simulation::motorEnergyConsumption()
        * (std::fabs(_motors[Motor::LEFT]) + std::fabs(_motors[Motor::RIGHT]));
  de *= _clockSpeed;
  de *= dt;

  // Check for regeneration
  decltype(_currHealth) mhA {0};
  float mh = 0;
  for (uint i=0; i<2*SPLINES_COUNT+1; i++) {
    mhA[i] = _masses[i] - _currHealth[i];
    assert(mhA[i] >= 0);
    mh += mhA[i];
  }
  if (mh > 0) {
    static const float h2ER = config::Simulation::healthToEnergyRatio();
//    std::cerr << "C" << id() << " missing health (total): " << mh << "\n";
    float r = config::Simulation::baselineRegenerationRate() * _clockSpeed * dt;
//    std::cerr << "Maximal energy for regeneration: " << r << "\n";
    r = std::min(r, mh / h2ER);
//    std::cerr << " Capped energy for regeneration: " << r << "\n";

    float r_ = r * h2ER;
    for (uint i=0; i<2*SPLINES_COUNT+1; i++) {
//      std::cerr << "\tAllocated " << r_ * mhA[i] / mh << " = " << r_
//                << " * " << mhA[i] << " / " << mh << std::endl;
      _currHealth[i] += r_ * mhA[i] / mh;
    }

    r -= r_ * mhA[0] / mh;  // Body health is kept as muscular energy
    de += r;
  }

  de = std::min(de, _energy);
  _energy -= de;
  env.modifyEnergyReserve(de);
}

void Critter::growthStep (void) { ERR

}

void Critter::reproduction (void) { ERR

}

void Critter::senescence (void) { ERR

}

#undef ERR

// =============================================================================
// == Internal shape-defining routines

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
  fd.restitution = .1;
  fd.friction = .3;

  FixtureData cfd (FixtureType::BODY, bodyColor());

  return addFixture(fd, cfd);
}

b2Fixture* Critter::addPolygonFixture (uint splineIndex, Side side,
                                       uint artifactIndex,
                                       const Vertices &v) {
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
  fd.restitution = .05;
  fd.friction = 0.;

  FixtureData cfd (FixtureType::ARTIFACT, splineColor(splineIndex),
                   splineIndex, side, artifactIndex);

  return addFixture(fd, cfd);
}

b2Fixture* Critter::addFixture (const b2FixtureDef &def,
                                const FixtureData &data) {

  b2Fixture *f = _body.CreateFixture(&def);
  auto pair = _b2FixturesUserData.emplace(f, data);
  if (!pair.second)
    utils::doThrow<std::logic_error>(
      "Unable to insert fixture ", data, " in collection");

  f->SetUserData(&pair.first->second);
  assert(f);
  return f;
}

void Critter::delFixture (b2Fixture *f) {
  _body.DestroyFixture(f);
  _b2FixturesUserData.erase(f);
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

  for (auto &v: collisionObjects) v.clear();

  if (_b2Body)  delFixture(_b2Body);
  _b2Body = addBodyFixture();

  b2MassData massData;  // Wastefull but doesn't seem to be a way around
  _b2Body->GetMassData(&massData);
  get(_b2Body)->centerOfMass = massData.center;
  _masses[0] = massData.mass;

  for (uint i=0; i<SPLINES_COUNT; i++) {
    if (dimorphism(i) == 0) continue;
    if (destroyedSpline(i, Side::LEFT) && destroyedSpline(i, Side::RIGHT))
      continue;

    std::vector<Vertices> objects;

    for (Side s: {Side::LEFT, Side::RIGHT}) {
      uint k = splineIndex(i, s);
      for (b2Fixture *f: _b2Artifacts[k]) delFixture(f);
      _b2Artifacts[k].clear();
    }
    _masses[1+splineIndex(i, Side::LEFT)] =
      _masses[1+splineIndex(i, Side::RIGHT)] = 0;

    // Sample spline at requested resolution and split concave quads
    const auto &d = _splinesData[i];
    std::array<P2D, N> p;
    for (uint t=0; t<P; t++)
      p[t] = pointAt(t, d.pl0, d.cl0, d.cl1, d.p1);
    for (uint t=1; t<P; t++)
      p[t+P-1] = pointAt(t, d.p1, d.cr0, d.cr1, d.pr0);

//    std::cerr << "Spline " << i << " points:";
//    for (const auto &p_: p) std::cerr << " " << p_;
//    std::cerr << std::endl;

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

    // Create right components
    uint n = objects.size();
    for (uint i=0; i<n; i++) {
      const Vertices &v = objects[i];
      Vertices v_;
      for (const P2D &p: v) v_.emplace_back(p.x, -p.y);
      objects.push_back(v_);
    }

    // TODO only create for non-destroyed splines
    // Create b2 object
    uint n2 = objects.size();
    for (uint j=0; j<n2; j++) {
      Side side = j<n ? Side::LEFT : Side::RIGHT;
      uint k = splineIndex(i, side);
      if (destroyedSpline(i, side)) continue;

      b2Fixture *f = addPolygonFixture(i, side, j%n, objects[j]);
      _b2Artifacts[k].push_back(f);

      f->GetMassData(&massData);
      _masses[1+k] += massData.mass;
      get(f)->centerOfMass = massData.center;

//      std::cerr << "C" << id() << "F" << *get(f)
//                << " COM: " << massData.center << " ("
//                << body().GetWorldPoint(massData.center) << ")" << std::endl;

//      std::cerr << "C" << id() << ": Spline " << side << i << " += "
//                << massData.mass << "; masses[1+" << k << "] = " << _masses[1+k]
//                << std::endl;
    }

//    // Update com for right-hand side fixtures
//    for (uint i=0; i<SPLINES_COUNT; i++) {
//      const auto &lA = _b2Artifacts[i], rA = _b2Artifacts[i+SPLINES_COUNT];
//      assert(lA.size() == rA.size());
//      for (uint j=0; j<lA.size(); j++) {
//        get(rA[j])->centerOfMass =
//            ySymmetrical(get(lA[j])->centerOfMass);
//      }
//    }

    // Register for debug
    for (uint j=0; j<n; j++) {
      collisionObjects[i].push_back(objects[j]);
      collisionObjects[i+SPLINES_COUNT].push_back(objects[j+n]);
    }
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
  return std::max(.5f, std::min(10/visionWidth, 20.f));
}

bool Critter::applyHealthDamage (const FixtureData &d, float amount) {
  uint i = 0;
  if (d.type != FixtureType::BODY) i += 1 + splineIndex(d.sindex, d.sside);
  float &v = _currHealth[i];

//  std::cerr << "Applying " << amount << " of damage to " << d
//            << ", resulting health is " << v << " >> ";

  v -= amount;
  utils::iclip_min(0.f, v);

//  std::cerr << v << std::endl;

  return v <= 0 && i > 0;
}

void Critter::destroySpline(b2Fixture *f) {
  const Critter::FixtureData &fd = *Critter::get(f);
  uint k = splineIndex(fd.sindex, fd.sside);

//  std::cerr << "Spline C" << id() << "S" << fd.sside << fd.sindex
//            << " was destroyed" << std::endl;

  _destroyed.set(k);
  _masses[1+k] = 0;

  for (b2Fixture *f_: _b2Artifacts[k]) delFixture(f_);
  _b2Artifacts[k].clear();

  collisionObjects[k].clear();
}

} // end of namespace simu
