#ifndef SIMU_CRITTER_H
#define SIMU_CRITTER_H

#include <bitset>

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

  static constexpr float MIN_SIZE = .2; // New-born size
  static constexpr float MAX_SIZE = 1;  // Adult size
  static constexpr float RADIUS = .5; // Body size of a mature splinoid

  static constexpr float BODY_DENSITY = 1;
  static constexpr float ARTIFACTS_DENSITY = 2;

  static constexpr auto SPLINES_COUNT = Genome::SPLINES_COUNT;
  static constexpr uint SPLINES_PRECISION = 4;

  enum class FixtureType { BODY, ARTIFACT };
  enum class Side : uint { LEFT = 0, RIGHT = 1 };
  struct FixtureData {
    FixtureType type;
    const Color &color;

    // Only valid if type == Artifact
    uint sindex;  // Index of spline [0,SPLINES_COUNT[
    Side sside;  // Side (left if original, right if mirrored)
    uint aindex;  // Index of artifact sub-component (UNUSED)

//    P2D centerOfMass; // Must be set after creation // WARNING Brittle

    FixtureData (FixtureType t, const Color &c,
                 uint si, Side fs, uint ai);
    FixtureData (FixtureType t, const Color &c)
      : FixtureData(t, c, -1, Side(-1), -1) {}

    friend std::ostream& operator<< (std::ostream &os, const FixtureData &fd);
    friend std::ostream& operator<< (std::ostream &os, const Side &s);
  };

private:
  Genome _genotype;

  float _visionRange; // Derived from genotype
  float _size; // in ]0,1] maturity-dependant

  b2Body &_body;
  b2BodyUserData _objectUserData;

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
  using SplinesData = std::array<SplineData, SPLINES_COUNT>;
  SplinesData _splinesData;

  b2Fixture *_b2Body;
  std::array<std::vector<b2Fixture*>, 2*SPLINES_COUNT> _b2Artifacts;
  std::map<b2Fixture*, FixtureData> _b2FixturesUserData;
  std::array<decimal, 1+2*SPLINES_COUNT> _masses;

  std::map<Motor, float> _motors;

  // Vision cache data (each vector of size 2*(2*genotype.vision.precision+1))
  std::vector<Color> _retina;
  std::array<P2D, 2> _raysStart;
  std::vector<P2D> _raysEnd;
  std::vector<float> _raysFraction; // TODO Remove (debug visu only)

  float _clockSpeed;
  float _age;

  float _efficiency, _ec0Coeff, _ec1Coeff;
  float _nextGrowthStep;

  decimal /*_maxEnergy,*/ _energy; // Only for main body

  decimal _reproductionReserve;
  b2Fixture *_reproductionSensor;

  /* Each portion managed independantly
   * Indices are
   *              0 body
   *          [1:S] left splines
   *   [S+1, 2*S-1] right splines
   * Where S in the number of splines (SPLINES_COUNT)
   */
  std::array<decimal, 1+2*SPLINES_COUNT> _currHealth;
  std::bitset<2*SPLINES_COUNT> _destroyed;

  // Potential extension
  //  float _water;

public:
  // TODO Remove these variables
  using Vertices = std::vector<P2D>;
  std::array<std::vector<Vertices>, 2*SPLINES_COUNT> collisionObjects;

  bool brainDead; // TODO for external control

  Critter(const Genome &g, b2Body *body, decimal e);

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

  static auto dimorphism (uint i, const Genome &g) {
    return g.dimorphism[4*g.sex()+i];
  }

  // Dimorphism coefficient for current sex and specified spline
  auto dimorphism (uint i) const {
    return dimorphism(i, _genotype);
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

  auto sizeRatio (void) const {
    return _size / MAX_SIZE;
  }

  auto bodyRadius (void) const {
    return _size * RADIUS;
  }

  auto bodyDiameter (void) const {
    return 2 * bodyRadius();
  }

  const auto& splinesData (void) const {
    return _splinesData;
  }

  const auto fixturesList (void) const {
    return _body.GetFixtureList();
  }

  auto mass (void) const {
    return _body.GetMass();
  }

  auto linearSpeed (void) const {
    return _body.GetLinearVelocity().Length();
  }

  auto angularSpeed (void) const {
    return _body.GetAngularVelocity();
  }

  auto efficiency (void) const {
    return _efficiency;
  }

  auto reproductionReserve (void) const {
    return _reproductionReserve;
  }

  auto reproductionReadiness (void) const {
    return _reproductionReserve / energyForCreation();
  }

  auto clockSpeed (void) const {
    return _clockSpeed;
  }

  static auto clockSpeed (const Genome &g, float v) {
    assert(0 <= v && v <= 1);
    return (1-v) * g.minClockSpeed
                       +    v  * g.maxClockSpeed;
  }

  // v in [0;1]
  auto clockSpeed (float v) {
    return _clockSpeed = clockSpeed(_genotype, v);
  }

  auto age (void) const {
    return _age;
  }

  auto matureAt (void) const {
    return _genotype.matureAge;
  }

  auto oldAt (void) const {
    return _genotype.oldAge;
  }

  bool isYouth (void) const {
    return _age < matureAt();
  }

  bool isAdult (void) const {
    return matureAt() <= _age && _age < oldAt();
  }

  bool isElder (void) const {
    return oldAt() <= _age;
  }

  decimal maxUsableEnergy (void) const {
    return _masses[0];
  }

  decimal usableEnergy (void) const {
    return _energy;
  }

  decimal totalEnergy (void) const {
    return _energy + config::Simulation::healthToEnergyRatio() * _currHealth[0];
  }

  auto storableEnergy (void) const {
    assert(usableEnergy() <= maxUsableEnergy());
    return maxUsableEnergy() - usableEnergy();
  }

  static auto splineIndex (uint i, Side side) {
    return uint(side) * SPLINES_COUNT + i;
  }

  static auto splineIndex (const FixtureData &fd) {
    return splineIndex(fd.sindex, fd.sside);
  }

  const auto& masses (void) const {
    return _masses;
  }

  const auto& health (void) const {
    return _currHealth;
  }

  auto bodyMaxHealth (void) const {
    return _masses[0];
  }

  auto splineMaxHealth (uint i, Side s) const {
    return _masses[1+splineIndex(i, s)];
  }

  auto bodyHealth (void) const {
    return _currHealth[0];
  }

  auto splineHealth (uint i, Side side) const {
    return _currHealth[1 + splineIndex(i, side)];
  }

  auto activeSpline (uint i, Side s) const {
    return splineMaxHealth(i, s) > 0;
  }

  auto destroyedSpline (uint i, Side s) const {
    return _destroyed.test(splineIndex(i, s));
  }

  bool applyHealthDamage(const FixtureData &d, float amount, Environment &env);
  void destroySpline(uint splineIndex);

  void updateShape (void);

  void setMotorOutput (float i, Motor m);

  float motorOutput (Motor m) const {
    return _motors.at(m);
  }

  void feed (decimal de) {
    assert(0 <= de && de <= maxUsableEnergy() - _energy);
    _energy += de;
  }

  const Color& bodyColor (void) const {
    return _genotype.colors[(SPLINES_COUNT+1)*sex()];
  }

  const Color& splineColor (uint i) const {
    return _genotype.colors[(SPLINES_COUNT+1)*sex()+i+1];
  }

  bool tooOld (void) const {
    return age() >= 1;
  }

  bool starved (void) const {
    return usableEnergy() <= 0;
  }

  bool fatallyInjured (void) const {
    return bodyHealth() <= 0;
  }

  bool isDead (void) const {
    return tooOld() || starved() || fatallyInjured();
  }

  void autopsy (void) const;

  // ===========================================================================
  // == Vision-related data

  auto visionBodyAngle (void) const {
    return _genotype.vision.angleBody;
  }

  auto visionRelativeRotation (void) const {
    return _genotype.vision.angleRelative;
  }

  auto visionWidth (void) const {
    return _genotype.vision.width;
  }

  auto visionPrecision (void) const {
    return _genotype.vision.precision;
  }

  float visionRange (void) const {
    return _visionRange;
  }

  const auto& retina (void) const {
    return _retina;
  }

  const auto& raysStart (void) const {
    return _raysStart;
  }

  const auto& raysEnd (void) const {
    return _raysEnd;
  }

  const auto &raysLength (void) const {
    return _raysFraction;
  }

  static b2BodyUserData* get (b2Body *b);
  static FixtureData* get (b2Fixture *f);

  // ===========================================================================
  // == Static computers

  static void setEfficiencyCoeffs(float c0, float &c0Coeff,
                                  float c1, float &c1Coeff);

  // age in [0,1]
  static float efficiency (float age,
                           float c0, float c0Coeff,
                           float c1, float c1Coeff);

  static float nextGrowthStepAt (uint currentStep);

  static decimal energyForCreation (void) {
    return 2*maximalEnergyStorage(MIN_SIZE);
  }

  static decimal maximalEnergyStorage (float size) {
    assert(MIN_SIZE <= size && size <= MAX_SIZE);
//    return size * BODY_DENSITY;
    return decimal(M_PI * size * size * RADIUS * RADIUS * BODY_DENSITY);
  }

  static float computeVisionRange (float visionWidth);

  static float agingSpeed (float clockSpeed);
  static float baselineEnergyConsumption (float size, float clockSpeed);

  static float lifeExpectancy (float clockSpeed);
  static float starvationDuration (float size, float energy, float clockSpeed);

private:
  // ===========================================================================
  // == Iteration substeps

  void drivingCorrections (void);
  void performVision (const Environment &env);
  void neuralStep (Environment &env);
  void energyConsumption (Environment &env);
  void regeneration (Environment &env);
  void aging (float dt);

  // ===========================================================================
  // == Shape-defining internal methods

  static void generateSplinesData (float r, float e, const Genome &g,
                                   SplinesData &d);
  void updateObjects (void);

  static void testConvex (const Vertices &o, std::vector<Vertices> &v);
  bool internalPolygon (const Vertices &v) const;

  bool insideBody (const P2D &p) const;

  b2Fixture* addBodyFixture (void);
  b2Fixture* addPolygonFixture(uint splineIndex, Side side, uint artifactIndex,
                               const Vertices &v);
  b2Fixture* addFixture (const b2FixtureDef &def,
                         const FixtureData &data);

  void delFixture (b2Fixture *f);

  // ===========================================================================
  // == Vision range managing internal methods
  void generateVisionRays (void);
  void updateVisionRays (void);

  // ===========================================================================
};

} // end of namespace simu

#endif // simu_CRITTER_H
