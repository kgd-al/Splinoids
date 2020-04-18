#include "critter.h"
#include "foodlet.h"
#include "environment.h"

#include "../hyperneat/phenotype.h"

namespace simu {

static constexpr int debugMotors = 0;
static constexpr int debugMetabolism = 0;
static constexpr int debugRegen = 0;
static constexpr int debugGrowth = 0;
static constexpr int debugReproduction = 0;

Critter::FixtureData::FixtureData (FixtureType t, const Color &c,
                                   uint si, Side fs, uint ai)
  : type(t), color(c), sindex(si), sside(fs), aindex(ai)/*, centerOfMass(P2D())*/ {}

Critter::FixtureData::FixtureData (FixtureType t, const Color &c)
  : FixtureData(t, c, -1, Side(-1), -1) {}

Critter::FixtureData::FixtureData (FixtureType t)
  : FixtureData(t, config::Simulation::obstacleColor(), -1, Side(-1), -1) {}

std::ostream& operator<< (std::ostream &os, const Critter::Side &s) {
  return os << (s == Critter::Side::LEFT ? "L" : "R");
}

std::ostream& operator<< (std::ostream &os, const Critter::FixtureData &fd) {
  if (fd.type == Critter::FixtureType::BODY)
    return os << "B";
  else
    return os << "S" << fd.sindex << fd.sside << fd.aindex;
}

Critter::FixtureData* Critter::get (b2Fixture *f) {
  return static_cast<FixtureData*>(f->GetUserData());
}

const Critter::FixtureData* Critter::get (const b2Fixture *f) {
  return static_cast<FixtureData*>(f->GetUserData());
}

b2BodyUserData* Critter::get (b2Body *b) {
  return static_cast<b2BodyUserData*>(b->GetUserData());
}

const b2BodyUserData* Critter::get (const b2Body *b) {
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

Critter::Critter(const Genome &g, b2Body *body, decimal e, float age)
  : _genotype(g), _size(MIN_SIZE), _body(*body) {

  static const decimal initEnergyRatio =
    1 + config::Simulation::healthToEnergyRatio();

  _visionRange = computeVisionRange(_genotype.vision.width);

  _b2Body = nullptr;

  _age = age;
  setEfficiencyCoeffs(matureAt(), _ec0Coeff, oldAt(), _ec1Coeff);

  if (age == 0) {
    _efficiency = 0;
    _nextGrowthStep = nextGrowthStepAt(0);

  } else {
    _efficiency = efficiency(_age, matureAt(), _ec0Coeff, oldAt(), _ec1Coeff);

    uint step = config::Simulation::growthSubsteps();
    if (isYouth()) {
      step *= _efficiency;
      _size = _efficiency * (MAX_SIZE - MIN_SIZE) + MIN_SIZE;

    } else
      _size = MAX_SIZE;

    _nextGrowthStep = nextGrowthStepAt(step);
  }

  _masses.fill(0);
  _currHealth.fill(0);
  _currentBodyColor = computeCurrentColor();

  updateShape();  

  _motors = { { Motor::LEFT, 0 }, { Motor::RIGHT, 0 } };
  clockSpeed(.5); assert(0 <= _clockSpeed && _clockSpeed < 10);
  _reproduction = 0;

  _energy = e / initEnergyRatio;

  _reproductionReserve = 0;
  _reproductionSensor = nullptr;

  // Update current healths
  _currHealth[0] = e * (1 - 1/initEnergyRatio) / config::Simulation::healthToEnergyRatio();
  for (uint i=0; i<SPLINES_COUNT; i++)
    for (Side s: {Side::LEFT, Side::RIGHT})
      _currHealth[1+splineIndex(i, s)] = splineMaxHealth(i, s);

  _destroyed.reset(0);

//  std::cerr << "Received " << e << " energy\n"
//            << bodyHealth() << " = " << bodyMaxHealth() << " * (1 - " << e
//              << " / " << initEnergyRatio << ")\n"
//            << CID(this) << " energy breakdown:\n"
//            << "\tReserve " << _energy << " / " << maxUsableEnergy() << "\n"
//            << "\t Health " << bodyHealth()
//              << " / " << bodyMaxHealth() << "\n"
//            << "\t  Total " << totalEnergy() << std::endl;

//  utils::iclip_max(_energy, _masses[0]);  // WARNING Loss of precision
  assert(_energy <= _masses[0]);
  assert(bodyHealth() <= bodyMaxHealth());

//  std::cerr << CID(this, "Splinoid ") << "\n";
//  std::cerr << "     Body health: " << bodyHealth() << " / " << bodyMaxHealth()
//            << "\n";

//  for (Side s: {Side::LEFT, Side::RIGHT})
//    for (uint i=0; i<SPLINES_COUNT; i++)
//      std::cerr << "Spline " << (s == Side::LEFT ? "L" : "R") << i
//                << " health: " << splineHealth(i, s) << " / "
//                << splineMaxHealth(i) << "\n";
//  std::cerr << std::endl;
  generateVisionRays();

  _objectUserData.type = BodyType::CRITTER;
  _objectUserData.ptr.critter = this;
  _body.SetUserData(&_objectUserData);

  brainDead = false;

  auto substrate = substrateFor(_raysEnd);
  _genotype.connectivity.BuildHyperNEATPhenotype(_brain, substrate);
}


void Critter::step(Environment &env) {
  // Driving improvement
  drivingCorrections();

  // Launch a bunch of rays
  performVision(env);

  // Query neural network
  neuralStep(env);

  // Distribute energy
  energyConsumption(env);

  // Regenerate body/artifacts as needed
  regeneration(env);

  // Age-specific tasks
  aging(env);

  // Udate appearance
  _currentBodyColor = computeCurrentColor();

  // TODO Smell?
}

void Critter::autopsy (void) const {
  if (!isDead())  std::cerr << "Please don't\n";
  else {
    std::cerr << CID(this, "Splinoid ") << " died of";
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

void Critter::drivingCorrections(void) {
  P2D velocity = _body.GetLinearVelocity();
  if (velocity.Length() < 1e-3) return;

  P2D tengentialNormal = _body.GetWorldVector({0,1}),
      lateralVelocity = b2Dot(tengentialNormal, velocity) * tengentialNormal;

  P2D zeroImpulse = -_body.GetMass() * lateralVelocity;
  _body.ApplyLinearImpulseToCenter(zeroImpulse, true);
}

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
        _retina[ie] = config::Simulation::obstacleColor();
        break;

      case BodyType::WARP_ZONE:
        _retina[ie] = config::Simulation::emptyColor();
        break;

      default:
        throw std::logic_error("Invalid body type");
      }

      _raysFraction[ie] = cvc.closestFraction;

    } else {
      _retina[ie] = config::Simulation::emptyColor();
      _raysFraction[ie] = 1;
    }
  }
}

void Critter::neuralStep(Environment &env) {  ERR
  if (!brainDead) {
    auto &dice = env.dice();

    // Set inputs
    std::vector<double> inputs;
    inputs.reserve(_retina.size() * std::tuple_size_v<Color> + 3);
    inputs.push_back(usableEnergy() / maxUsableEnergy());
    inputs.push_back(bodyHealthness());
    inputs.push_back(reproductionReadiness());
//    inputs.push_back(dice(-1,1));
    for (const Color &c: _retina) for (float v: c)  inputs.push_back(v);

    // Process n propagation steps
    _brain.Input(inputs);
    for (uint i=0; i<_genotype.brainSubsteps; i++)  _brain.Activate();
    std::vector<double> outputs = _brain.Output();

    // Collect outputs
    for (double &v: outputs)  assert(0 <= v && v <= 1);
    _motors[Motor::LEFT] = outputs[0];
    _motors[Motor::RIGHT] = outputs[1];
    _clockSpeed = clockSpeed(outputs[2]);
    _reproduction = outputs[3];

    if (false) {
      auto motorsIn = _motors;
      _motors[Motor::LEFT] = _motors[Motor::RIGHT] = 0;

//#define DEBUG(X) if (false && id() == ID(X))  std::cerr << "[" << CID(this) << "] "

//    // Feed. Mate. Repeat
//    const auto &r = _retina;
//    bool foodLeft = (r[0][0] == 0 && r[0][2] == 0 && r[0][1] >= .75);
//    bool foodRight = (r[1][0] == 0 && r[1][2] == 0 && r[1][1] >= .75);

//    bool mateLeft = (r[0][0] >= .75 && r[0][2] <= .75 && r[0][1] <= .75);
//    bool mateRight = (r[1][0] >= .75 && r[1][2] <= .75 && r[1][1] <= .75);

//    bool obstacleLeft = (r[0][0] < 1 || r[0][1] < 1 || r[0][2] < 1);
//    bool obstacleRight = (r[1][0] < 1 || r[1][1] < 1 || r[1][2] < 1);

//    if (f < .75 && (foodLeft || foodRight)) { // Go to food
//      DEBUG(1) << "Going to food" << std::endl;
//      if (foodLeft)   _motors[Motor::LEFT] = 1;
//      if (foodRight)  _motors[Motor::RIGHT] = 1;

//    } else if (s == 1 && (mateLeft || mateRight)) { // Go to mate
//      DEBUG(1) << "Going to mate" << std::endl;
//      if (mateLeft)   _motors[Motor::LEFT] = 1;
//      if (mateRight)  _motors[Motor::RIGHT] = 1;

//    } else if (obstacleLeft) {  // Evade right
//      DEBUG(1) << "Evading something on the left" << std::endl;
//      _motors[Motor::LEFT] = -.1;
//      _motors[Motor::RIGHT] = .1;

//    } else if (obstacleRight) { // Evade left
//      DEBUG(1) << "Evading something on the right" << std::endl;
//      _motors[Motor::LEFT] = .1;
//      _motors[Motor::RIGHT] = -.1;

//    } else if (dice(.1)){  // Walk around
//      DEBUG(1) << "Walking around" << std::endl;
//      if (dice(.6))
//        _motors[Motor::LEFT] = _motors[Motor::RIGHT] = 1;
//      else if (dice(.2))
//        _motors[Motor::LEFT] = _motors[Motor::RIGHT] = -1;
//      else {
//        int d = dice.toss(-1,1);
//        _motors[Motor::LEFT] = d;
//        _motors[Motor::RIGHT] = -d;
//      }

//    } else {
//      DEBUG(1) << "Idling" << std::endl;
//      _motors[Motor::LEFT] = motorsIn[Motor::LEFT];
//      _motors[Motor::RIGHT] = motorsIn[Motor::RIGHT];
//    }

//    _reproduction = 1;
    }
  }

  // Apply requested motor output
  for (auto &m: _motors) {
    float s = config::Simulation::critterBaseSpeed()
        * m.second * _clockSpeed * _efficiency * _size;
    P2D f = _body.GetWorldVector({s,0}),
        p = _body.GetWorldPoint({0, int(m.first)*.5f*bodyRadius()});
    _body.ApplyForce(f, p, true);

    if (debugMotors)
      std::cerr << CID(this) << " Applied motor force for " << m.first
                << " of " << s << " = "
                << config::Simulation::critterBaseSpeed() << " * "
                << m.second << " * " << _clockSpeed << " * "
                << _efficiency << " * " << _size << std::endl;
  }

#undef DEBUG
}

void Critter::energyConsumption (Environment &env) {
  decimal dt = env.dt();
  decimal de = 0;

  de += baselineEnergyConsumption(_size, _clockSpeed);
  de += config::Simulation::motorEnergyConsumption()
        * (std::fabs(_motors[Motor::LEFT]) + std::fabs(_motors[Motor::RIGHT]))
        * _clockSpeed * _size;
  de *= dt;

  if (debugMetabolism)
    std::cerr << CID(this) << " de = " << de << " = "
              << baselineEnergyConsumption(_size, _clockSpeed) << " + "
              << config::Simulation::motorEnergyConsumption() << " * "
              << _clockSpeed << " * " << _size << " * " << dt << std::endl;

  de = std::min(de, _energy);
  _energy -= de;
  env.modifyEnergyReserve(de);
}

void Critter::regeneration (Environment &env) {
  decimal dt = env.dt();
  decimal dE = 0;

  // Check for regeneration
  decltype(_currHealth) mhA {0};
  decimal mh = 0;
  for (uint i=0; i<2*SPLINES_COUNT+1; i++) {
    mhA[i] = _masses[i] - _currHealth[i];
    assert(mhA[i] >= 0);
    mh += mhA[i];
  }

  if (mh == 0)  return;

  static const decimal h2ER = config::Simulation::healthToEnergyRatio();

  if (debugRegen)
    std::cerr << CID(this) << " missing health (total): " << mh << "\n";

  dE = _energy * config::Simulation::baselineRegenerationRate()
      * _clockSpeed * dt;

  if (debugRegen)
    std::cerr << "Maximal energy for regeneration: " << std::min(dE, mh)
              << " = min(" << dE << ", " << mh << ") = min(" << _energy << " * "
              << config::Simulation::baselineRegenerationRate() << " * "
              << _clockSpeed << " * " << dt << ", ...)\n";

  dE = std::min(dE, mh);

  for (uint i=0; i<2*SPLINES_COUNT+1; i++) {
    if (mhA[i] == 0)  continue;

    if (debugRegen > 2)
      std::cerr << "\t\t(B) health: " << _currHealth[i] << " / "
                << _masses[i] << " (" << 100*_currHealth[i]/_masses[i]
                << "%)\n";

    if (debugRegen > 1)
      std::cerr << "\t[" << i << "] Regenerated " << dE * mhA[i] / mh << " = "
                << dE << " * " << 100*mhA[i]/mh << "% (" << mhA[i] << " / "
                << mh << ")" << std::endl;

    _currHealth[i] += dE * mhA[i] / mh;

    if (debugRegen > 2)
      std::cerr << "\t\t(A) health: " << _currHealth[i] << " / "
                << _masses[i] << " (" << 100*_currHealth[i]/_masses[i]
                << "%)\n";
  }

  _energy -= dE;
  env.modifyEnergyReserve(dE - dE * h2ER * mhA[0] / mh);
}

void Critter::aging(Environment &env) {
  float dt = env.dt();

  _age += dt * agingSpeed(_clockSpeed);

  auto prevEfficiency = _efficiency;
  _efficiency = efficiency(_age, matureAt(), _ec0Coeff, oldAt(), _ec1Coeff);

  // Immature critter. Growth threshold crossed -> update
  if (_nextGrowthStep <= _efficiency && prevEfficiency < _nextGrowthStep) {
    uint step = _efficiency * config::Simulation::growthSubsteps();

    if (debugGrowth)
      std::cerr << CID(this) << " should grow (step " << step << ")"
                << std::endl;

    float newSize = _efficiency * (MAX_SIZE - MIN_SIZE) + MIN_SIZE;

    if (debugGrowth > 1) {
      std::cerr << "\tSize: " << _size << " >> " << newSize << std::endl;

      std::cerr << "\tMasses:";
      for (uint i=0; i<_masses.size(); i++) {
        if (_masses[i] > 0)
          std::cerr << " " << _currHealth[i] << " / " << _masses[i];
        else
          std::cerr << " 0";
      }
      std::cerr << "\n";
    }

    _size = newSize;
    updateShape();
    updateVisionRays();
    _nextGrowthStep = nextGrowthStepAt(step);

    if (debugGrowth > 1) {
      std::cerr << "\tSize: " << _size << " >> " << newSize << std::endl;

      std::cerr << "\tMasses:";
      for (uint i=0; i<_masses.size(); i++) {
        if (_masses[i] > 0)
          std::cerr << " " << _currHealth[i] << " / " << _masses[i];
        else
          std::cerr << " 0";
      }
      std::cerr << "\n\tnext growth step at " << _nextGrowthStep << "\n";
    }
  }

  // Mature critter. If first step, create reproduction sensor
  //  Only register change of state, accumulation is performed by neural step
  //  and actual reproduction by the environment
  if (_efficiency == 1) {
    if (prevEfficiency < 1 && (debugGrowth || debugReproduction))
      std::cerr << CID(this) << " turned adult." << std::endl;

    decimal dE = _energy * config::Simulation::baselineGametesGrowth()
               * _clockSpeed * dt;
    dE = std::min(dE, _energy);
    dE = std::min(dE, energyForChild() - _reproductionReserve);
    _energy -= dE;
    _reproductionReserve += dE;
    env.modifyEnergyReserve(+dE);

    // Just turned active -> create sensor
    if (reproductionReadiness() == 1 && _reproductionSensor == nullptr)
      _reproductionSensor = addReproFixture();
  }

  // Old critter. Destroy reproduction sensor
  if (oldAt() < _age && prevEfficiency == 1) {
    if (debugGrowth || debugReproduction)
      std::cerr << CID(this) << " grew old." << std::endl;
    resetMating();
  }
}

void Critter::feed (Foodlet *f, float dt) {
  static const decimal &dE = config::Simulation::energyAbsorptionRate();

  decimal E = dE * _clockSpeed * _efficiency * dt;
  E = std::min(E, f->energy());
  E = std::min(E, storableEnergy());

  if (debugMetabolism > 1)
    std::cerr << "Transfering " << E << " = min(" << f->energy() << ", "
              << storableEnergy() << ", " << dE << " * " << _clockSpeed
              << " * " << _efficiency << " * " << dt << ") from " << f->id()
              << " to " << CID(this) << " (" << f->energy()
              << " remaining)" << std::endl;

  f->consumed(E);

  assert(0 <= E && E <= storableEnergy());
  _energy += E;
}

#undef ERR

// =============================================================================
// == Internal shape-defining routines


void Critter::updateShape(void) {
  generateSplinesData(bodyRadius(), _efficiency, _genotype, _splinesData);
  updateObjects();
}

#ifndef NDEBUG
#define SET(LHS, RHS) LHS = RHS;
#else
#define SET(LHS, RHS)
#endif

void Critter::generateSplinesData (float r, float e, const Genome &g,
                                   SplinesData &sda) {
  using S = genotype::Spline::Index;
  static constexpr auto W0_RANGE = M_PI/2;

  for (uint i=0; i<SPLINES_COUNT; i++) {
    const genotype::Spline::Data &d = g.splines[i].data;
    SplineData &sd = sda[i];
    float w = dimorphism(i, g) * e;

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

// Taken straight from b2_polygon.cpp
// Modified to return bool instead of asserting
bool box2dValidPolygon(const b2Vec2* vertices, int32 count) {
  b2Assert(3 <= count && count <= b2_maxPolygonVertices);
  if (count < 3)
  {
//    SetAsBox(1.0f, 1.0f);
    return false;
  }

  int32 n = b2Min(count, b2_maxPolygonVertices);

  // Perform welding and copy vertices into local buffer.
  b2Vec2 ps[b2_maxPolygonVertices];
  int32 tempCount = 0;
  for (int32 i = 0; i < n; ++i)
  {
    b2Vec2 v = vertices[i];

    bool unique = true;
    for (int32 j = 0; j < tempCount; ++j)
    {
      if (b2DistanceSquared(v, ps[j]) < ((0.5f * b2_linearSlop) * (0.5f * b2_linearSlop)))
      {
        unique = false;
        break;
      }
    }

    if (unique)
    {
      ps[tempCount++] = v;
    }
  }

  n = tempCount;
  if (n < 3)
  {
    // Polygon is degenerate.
//    b2Assert(false);
//    SetAsBox(1.0f, 1.0f);
    return false;
  }

  // Create the convex hull using the Gift wrapping algorithm
  // http://en.wikipedia.org/wiki/Gift_wrapping_algorithm

  // Find the right most point on the hull
  int32 i0 = 0;
  float x0 = ps[0].x;
  for (int32 i = 1; i < n; ++i)
  {
    float x = ps[i].x;
    if (x > x0 || (x == x0 && ps[i].y < ps[i0].y))
    {
      i0 = i;
      x0 = x;
    }
  }

  int32 hull[b2_maxPolygonVertices];
  int32 m = 0;
  int32 ih = i0;

  for (;;)
  {
    b2Assert(m < b2_maxPolygonVertices);
    hull[m] = ih;

    int32 ie = 0;
    for (int32 j = 1; j < n; ++j)
    {
      if (ie == ih)
      {
        ie = j;
        continue;
      }

      b2Vec2 r = ps[ie] - ps[hull[m]];
      b2Vec2 v = ps[j] - ps[hull[m]];
      float c = b2Cross(r, v);
      if (c < 0.0f)
      {
        ie = j;
      }

      // Collinearity check
      if (c == 0.0f && v.LengthSquared() > r.LengthSquared())
      {
        ie = j;
      }
    }

    ++m;
    ih = ie;

    if (ie == i0)
    {
      break;
    }
  }

  if (m < 3)
  {
    // Polygon is degenerate.
//    b2Assert(false);
//    SetAsBox(1.0f, 1.0f);
    return false;
  }

//  m_count = m;

//  // Copy vertices.
//  for (int32 i = 0; i < m; ++i)
//  {
//    m_vertices[i] = ps[hull[i]];
//  }

//  // Compute normals. Ensure the edges have non-zero length.
//  for (int32 i = 0; i < m; ++i)
//  {
//    int32 i1 = i;
//    int32 i2 = i + 1 < m ? i + 1 : 0;
//    b2Vec2 edge = m_vertices[i2] - m_vertices[i1];
//    b2Assert(edge.LengthSquared() > b2_epsilon * b2_epsilon);
//    m_normals[i] = b2Cross(edge, 1.0f);
//    m_normals[i].Normalize();
//  }

//  // Compute the polygon centroid.
//  m_centroid = ComputeCentroid(m_vertices, m);

  return true;
}


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
  fd.filter.categoryBits = uint16(CollisionFlag::CRITTER_BODY_FLAG);
  fd.filter.maskBits = uint16(CollisionFlag::CRITTER_BODY_MASK);

  FixtureData cfd (FixtureType::BODY, _currentBodyColor);

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

  if (!box2dValidPolygon(v.data(), v.size())) return nullptr;

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
  fd.filter.categoryBits = uint16(CollisionFlag::CRITTER_SPLN_FLAG);
  fd.filter.maskBits = uint16(CollisionFlag::CRITTER_SPLN_MASK);

  FixtureData cfd (FixtureType::ARTIFACT, splineColor(splineIndex),
                   splineIndex, side, artifactIndex);

  return addFixture(fd, cfd);
}

b2Fixture* Critter::addReproFixture(void) {
  assert(_reproductionSensor == nullptr);
  b2CircleShape s;
  s.m_p.Set(0, 0);
  s.m_radius = config::Simulation::reproductionRange() * bodyRadius();

  b2FixtureDef fd;
  fd.shape = &s;
  fd.density = 0;
  fd.isSensor = true;
  fd.filter.categoryBits = uint16(CollisionFlag::CRITTER_REPRO_FLAG);
  fd.filter.maskBits = uint16(CollisionFlag::CRITTER_REPRO_MASK);

  FixtureData cfd (FixtureType::REPRODUCTION);
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
  return std::sqrt(p.x*p.x+p.y*p.y) <= bodyRadius() + 1e-3;
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
//  get(_b2Body)->centerOfMass = massData.center;
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
      if (!f) continue; // nullptr is returned is box2dValidPolygon returns false

      _b2Artifacts[k].push_back(f);

      f->GetMassData(&massData);
      _masses[1+k] += massData.mass;
//      get(f)->centerOfMass = massData.center;

//      std::cerr << CID(this) << "F" << *get(f)
//                << " COM: " << massData.center << " ("
//                << body().GetWorldPoint(massData.center) << ")" << std::endl;

//      std::cerr << CID(this) << ": Spline " << side << i << " += "
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

  if (config::Simulation::b2FixedBodyCOM()) {
    b2MassData d;
    _body.GetMassData(&d);
    d.center = {0,0};
    _body.SetMassData(&d);
  }
}

// =============================================================================
// == Unsorted stuff

Color Critter::computeCurrentColor(void) const {
  Color color = initialBodyColor();
  static constexpr auto CM = Genome::config_t::COLOR_MIN;
  float h = bodyHealthness();
  if (h < 1)  for (float &v: color) v = CM + h * (v-CM);
  if (requestingMating()) color[0] = 1;

//  using utils::operator <<;
//  std::cerr << CID(this) << " body color: " << color << std::endl;

  return color;
}

void Critter::setMotorOutput(float i, Motor m) {
  assert(-1 <= i && i <= 1);
  assert(EnumUtils<Motor>::isValid(m));
  _motors.at(m) = i;
}

bool Critter::applyHealthDamage (const FixtureData &d, float amount,
                                 Environment &env) {
  decimal damount = amount;

  uint i = 0;
  if (d.type != FixtureType::BODY) i += 1 + splineIndex(d.sindex, d.sside);
  decimal &v = _currHealth[i];

//  std::cerr << "Applying " << damount << " of damage to " << CID(this) << d
//            << ", resulting health is " << v << " >> ";

  damount = std::min(damount, v);
  v -= damount;

  if (d.type == FixtureType::BODY)  // Return energy to environment
    env.modifyEnergyReserve(+damount*config::Simulation::healthToEnergyRatio());

//  std::cerr << v << "\n";
//  if (d.type == FixtureType::BODY)  // Return energy to environment
//    std::cerr << "\tReturning " << damount*config::Simulation::healthToEnergyRatio()
//              << " energy to the environment\n";
//  std::cerr << std::endl;

  return v <= 0 && i > 0;
}

void Critter::destroySpline(uint splineIndex) {
  uint k = splineIndex;

//  std::cerr << "Spline " << CID(this) << "S" << fd.sside << fd.sindex
//            << " was destroyed" << std::endl;

  _destroyed.set(k);
  _masses[1+k] = 0;

  for (b2Fixture *f_: _b2Artifacts[k]) delFixture(f_);
  _b2Artifacts[k].clear();

  collisionObjects[k].clear();

  if (config::Simulation::b2FixedBodyCOM()) {
    b2MassData d;
    _body.GetMassData(&d);
    d.center = {0,0};
    _body.SetMassData(&d);
  }
}

void Critter::generateVisionRays(void) {
  const auto &v = _genotype.vision;
  uint rs_h = 2 * v.precision + 1;
  uint rs = 2 * rs_h;
  _retina.resize(rs, Color());
  _raysEnd.resize(rs, P2D(0,0));
  _raysFraction.resize(rs, 1);

  float a0 = v.angleBody;
  _raysStart[0] = fromPolar(a0, bodyRadius());
  _raysStart[1] = ySymmetrical(_raysStart[0]);

  float r = _visionRange;
  for (uint i=0; i<rs_h; i++) {
    float da = 0;
    if (v.precision > 0) da = .5 * v.width * (i / float(v.precision) - 1);
    float a = a0 + v.angleRelative + da;
//    std::cerr << "[" << i << "] a0 = "
//              << 180*a0/M_PI << "; da = " << 180*da/M_PI
//              << "; a = " << 180*a/M_PI << std::endl;

    uint il = rs_h-i-1, ir = rs_h+i;
    _raysEnd[il] = _raysStart[0] + fromPolar(a, r);
    _raysEnd[ir] = ySymmetrical(_raysEnd[il]);

//    std::cerr << "E[" << il << "] = " << _raysEnd[il]
//              << " = " << _raysStart[0] << " + "
//              << r * P2D(std::cos(a), std::sin(a)) << std::endl;
//    std::cerr << "E[" << ir << "] = " << _raysEnd[ir]
//                 << " = { " << _raysEnd[il].x << ", " << -_raysEnd[il].y
//                 << " }" << std::endl;
//    std::cerr << std::endl;
  }
//  std::cerr << std::endl;
}

void Critter::updateVisionRays(void) {
  float a0 = _genotype.vision.angleBody;
  P2D rs0 = fromPolar(a0, bodyRadius());
  P2D d = rs0 - _raysStart[0];
  _raysStart[0] = rs0;
  _raysStart[1] = ySymmetrical(rs0);

  uint nh = _raysEnd.size()/2;
  for (uint i=0; i<2; i++) {
    for (uint j=0; j<nh; j++) {
      _raysEnd[j] += d;
      _raysEnd[j+nh] = ySymmetrical(_raysEnd[j]);
    }
  }
}

void Critter::setEfficiencyCoeffs(float c0, float &c0Coeff,
                                  float c1, float &c1Coeff) {
  static constexpr float e = 1e-3;
  c0Coeff = std::atanh(1-e)/c0;
  c1Coeff = std::sqrt(-(1-c1)*(1-c1)/(2*std::log(e)));
}

float Critter::efficiency(float age,
                          float c0, float c0Coeff,
                          float c1, float c1Coeff) {

  if (age < c0)       return std::tanh(age * c0Coeff);
  else if (age < c1)  return 1;
  else                return utils::gauss(age, c1, c1Coeff);
}

float Critter::nextGrowthStepAt (uint currentStep) {
  static const float N = config::Simulation::growthSubsteps();
  assert(currentStep <= N);
  return (currentStep+1) / N;
}

float Critter::computeVisionRange(float visionWidth) {
  return std::max(.5f, std::min(5/visionWidth, 20.f));
}

float Critter::agingSpeed (float clockSpeed) {
  return config::Simulation::baselineAgingSpeed() * clockSpeed;
}

float Critter::baselineEnergyConsumption (float size, float clockSpeed) {
  return config::Simulation::baselineEnergyConsumption()
      * clockSpeed * size;
}

float Critter::lifeExpectancy (float clockSpeed) {
  return 1. / agingSpeed(clockSpeed);
}

float Critter::starvationDuration (float size, float energy, float clockSpeed) {
  return energy / baselineEnergyConsumption(size, clockSpeed);
}

void save (nlohmann::json &j, const NEAT::NeuralNetwork &ann) {
  nlohmann::json jn, jc;
  for (const NEAT::Neuron &n: ann.m_neurons)
    jn.push_back({ n.m_type, n.m_a, n.m_b, n.m_timeconst, n.m_bias,
                   n.m_activation_function_type, n.m_split_y });
  for (const NEAT::Connection &c: ann.m_connections)
    jc.push_back({ c.m_source_neuron_idx, c.m_target_neuron_idx,
                   c.m_weight, c.m_recur_flag,
                   c.m_hebb_rate, c.m_hebb_pre_rate });
  j = { jn, jc };
}

void load (const nlohmann::json &j, NEAT::NeuralNetwork &ann) {
  for (const auto &jn: j[0]) {
    NEAT::Neuron n;
    uint i=0;
    n.m_type = jn[i++];
    n.m_a = jn[i++];
    n.m_b = jn[i++];
    n.m_timeconst = jn[i++];
    n.m_bias = jn[i++];
    n.m_activation_function_type = jn[i++];
    n.m_split_y = jn[i++];
    ann.m_neurons.push_back(n);
  }

  for (const auto &jc: j[1]) {
    NEAT::Connection c;
    uint i=0;
    c.m_source_neuron_idx = jc[i++];
    c.m_target_neuron_idx = jc[i++];
    c.m_weight = jc[i++];
    c.m_recur_flag = jc[i++];
    c.m_hebb_rate = jc[i++];
    c.m_hebb_pre_rate = jc[i++];
    ann.m_connections.push_back(c);
  }
}

nlohmann::json Critter::save (const Critter &c) {
  nlohmann::json jb;
  simu::save(jb, c._brain);
  return nlohmann::json {
    c._genotype, jb, c._energy, c._age, c._reproductionReserve,
    c._currHealth, c._destroyed.to_string()
  };
}

Critter* Critter::load (const nlohmann::json &j, b2Body *body) {
  Critter *c = new Critter (j[0], body, j[2], j[3]);
  simu::load(j[1], c->_brain);
  c->_energy = j[2];
  c->_reproductionReserve = j[4];
  c->_currHealth = j[5];
  c->_destroyed = decltype(c->_destroyed)(j[6].get<std::string>());

  return c;
}

} // end of namespace simu
