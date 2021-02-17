#ifndef SIMU_CRITTER_H
#define SIMU_CRITTER_H

#include <bitset>

#include "kgd/eshn/phenotype/ann.h"

#include "../genotype/critter.h"
#include "config.h"

#include "box2d/b2_body.h"

#include "../enumarray.hpp"

DEFINE_PRETTY_ENUMERATION(Motor, LEFT = 1, RIGHT = -1)

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

  static constexpr float MIN_DENSITY = 1;
  static constexpr float MAX_DENSITY = 2;

  static constexpr auto SPLINES_COUNT = Genome::SPLINES_COUNT;
  static constexpr uint SPLINES_PRECISION = 4;

  static constexpr uint VOCAL_CHANNELS = 3;

  using FixtureType_ut = uint8;
  enum class FixtureType : FixtureType_ut {
    BODY = 0x1,
    ARTIFACT = 0x2,
    AUDITION = 0x4,
    REPRODUCTION = 0x8
  };
  friend FixtureType operator| (const FixtureType lhs, const FixtureType &rhs) {
    return FixtureType(FixtureType_ut(lhs) | FixtureType_ut(rhs));
  }
  friend FixtureType operator& (const FixtureType lhs, const FixtureType &rhs) {
    return FixtureType(FixtureType_ut(lhs) & FixtureType_ut(rhs));
  }

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
    FixtureData (FixtureType t, const Color &c);
    FixtureData (FixtureType t);

    friend std::ostream& operator<< (std::ostream &os, const FixtureData &fd);
    friend std::ostream& operator<< (std::ostream &os, const Side &s);

    friend void assertEqual (const FixtureData &lhs, const FixtureData &rhs,
                             bool deepcopy) {
      using utils::assertEqual;
#define ASRT(X) assertEqual(lhs.X, rhs.X, deepcopy)
      ASRT(type);
      ASRT(color);
      ASRT(sindex);
      ASRT(sside);
      ASRT(aindex);
#undef ASRT
    }
  };

private:
  Genome _genotype;

  float _visionRange; // Derived from genotype
  float _size; // in ]0,1] maturity-dependant

  b2Body &_body;
  b2BodyUserData _objectUserData;
  Color _currentBodyColor;

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
    P2D cr1;  // Second control point'

#ifndef NDEBUG
    friend void assertEqual (const SplineData &lhs, const SplineData &rhs,
                             bool deepcopy) {
      using utils::assertEqual;
#define ASRT(X) assertEqual(lhs.X, rhs.X, deepcopy)
      ASRT(p0);
      ASRT(c0);
      ASRT(c1);
      ASRT(p1);
      ASRT(pc0);
      ASRT(pc1);
      ASRT(al0);
      ASRT(ar0);
      ASRT(pl0);
      ASRT(cl0);
      ASRT(cl1);
      ASRT(pr0);
      ASRT(cr0);
      ASRT(cr1);
#undef ASRT
    }
#endif
  };
  using SplinesData = std::array<SplineData, SPLINES_COUNT>;
  SplinesData _splinesData;

  b2Fixture *_b2Body;
  std::array<std::vector<b2Fixture*>, 2*SPLINES_COUNT> _b2Artifacts;
  std::map<b2Fixture*, FixtureData> _b2FixturesUserData;
  std::array<decimal, 1+2*SPLINES_COUNT> _masses;

  // ===========================================================================
  // == Vision cache data ==
  // (each vector of size 2*(2*genotype.vision.precision+1))

  std::vector<Color> _retina;
  std::array<P2D, 2> _raysStart;
  std::vector<P2D> _raysEnd;
  std::vector<float> _raysFraction; // TODO Remove (debug visu only)

  // ===========================================================================
  // == Audition cache data ==
  std::array<float, 2*(VOCAL_CHANNELS+1)> _ears;

  // ===========================================================================
  // == Neural outputs ==
  std::map<Motor, float> _motors;
  float _clockSpeed;
  std::array<float, 2> _voice;
  float _reproduction;

  // ===========================================================================
  // == Other ==

  phenotype::ANN _brain;

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

  // Cached-data for sounds emitted into the environment
  // 0 -> Involuntary (motion, feeding, ...)
  // 1 -> Vocalisations
  std::array<float, VOCAL_CHANNELS+1> _sounds;
  b2Fixture *_auditionSensor;

  // To monitor feeding behavior
  using FeedingSources = utils::enumarray<float, BodyType,
                                                  BodyType::PLANT,
                                                  BodyType::CORPSE>;
  FeedingSources _feedingSources;

  // To monitor behavior
  decltype(std::declval<phenotype::ANN>().outputs()) _neuralOutputs;

  // Potential extension
  //  float _water;

public:
  // TODO Remove these variables
  using Vertices = std::vector<P2D>;
  std::array<std::vector<Vertices>, 2*SPLINES_COUNT> collisionObjects;

  bool brainDead; // TODO for external control

  uint userIndex;  // To monitor source population

  Critter(const Genome &g, b2Body *body, decimal e, float age = 0);

  void step (Environment &env);

  const auto& genotype (void) const {
    return _genotype;
  }

  auto& genotype (void) {
    return _genotype;
  }

  auto& genealogy (void) {
    return _genotype.genealogy();
  }

  auto id (void) const {
    return genotype().id();
  }

  auto species (void) const {
    return genotype().species();
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

  auto reproductionType (void) const {
    return Genome::ReproductionType(_genotype.asexual);
  }

  bool hasSexualReproduction (void) const {
    return reproductionType() == Genome::SEXUAL;
  }

  bool hasAsexualReproduction (void) const {
    return reproductionType() == Genome::ASEXUAL;
  }

  auto reproductionReserve (void) const {
    return _reproductionReserve;
  }

  auto reproductiveInvestment (Genome::ReproductionType t) const {
    return (t+1)*_genotype.reproductiveInvestment();
  }

  auto energyForChild (Genome::ReproductionType t) const {
    return reproductiveInvestment(t) * energyForCreation();
  }

  // For now both contribute the same
  auto reproductionReadiness (Genome::ReproductionType t) const {
    return _reproductionReserve / energyForChild(t);
  }

  // Uses "current" scheme
  auto reproductionReadinessBrittle (void) const {
    return _reproductionReserve / energyForChild(reproductionType());
  }

  bool requestingMating (Genome::ReproductionType t) const {
    return reproductionReadiness(t) == 1
        && _reproduction > config::Simulation::reproductionRequestThreshold();
  }

  const auto reproductionSensor (void) const {
    return _reproductionSensor;
  }

  auto reproductionOutput (void) const {
    return _reproduction;
  }

  void resetMating (void) {
    _reproductionReserve = 0;
    if (_reproductionSensor) {
      delFixture(_reproductionSensor);
      _reproductionSensor = nullptr;
    }
  }

  auto clockSpeed (void) const {
    return _clockSpeed;
  }

  static auto clockSpeed (const Genome &g, float v) {
    assert(0 <= v && v <= 1);
    return (1-v) * g.minClockSpeed + v * g.maxClockSpeed;
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

  void overrideUsableEnergyStorage (decimal newValue) {
    _energy = newValue;
  }

  void overrideReproductionReserve (decimal newValue) {
    _reproductionReserve = newValue;
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

  auto bodyHealthness (void) const {
    return bodyHealth() / bodyMaxHealth();
  }

  auto splineHealthness (uint i, Side s) const {
    return splineHealth(i, s) / splineMaxHealth(i, s);
  }

  const auto& healthArray (void) const {
    return _currHealth;
  }

  decimal maxOutHealthAndEnergy (void);

  auto activeSpline (uint i, Side s) const {
    return splineMaxHealth(i, s) > 0;
  }

  auto destroyedSpline (uint i, Side s) const {
    return _destroyed.test(splineIndex(i, s));
  }

  const auto& brain (void) const {
    return _brain;
  }

  const auto& feedingSources (void) const {
    return _feedingSources;
  }

  float carnivorousBehavior (void) const {
    float total = 0;
    for (float f: _feedingSources)  total += f;
    return _feedingSources[BodyType::CORPSE] / total;
  }

  bool applyHealthDamage(const FixtureData &d, float amount, Environment &env);
  void destroySpline(uint splineIndex);

  void updateShape (void);

  void setMotorOutput (float i, Motor m);

  float motorOutput (Motor m) const {
    return _motors.at(m);
  }

  const auto& neuralOutputs (void) const {
    return _neuralOutputs;
  }

  const auto& neuralOutputsHeader (void) const {
    static const std::vector<std::string> v {
      "LMotor", "RMotor", "Clock", "Repro"
    };
    return v;
  }

  void feed (Foodlet *f, float dt);

  const Color& currentBodyColor (void) const {
    return _currentBodyColor;
  }

  const Color& initialBodyColor (void) const {
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

  // ===========================================================================
  // == Audition/Vocalisation data

  auto& ears (void) {
    return _ears;
  }

  const auto& ears (void) const {
    return _ears;
  }

  bool silent (void) const {
    return std::all_of(_sounds.begin(), _sounds.end(),
                       [] (auto v) { return v == 0; });
  }

  void setVocalisation(float v, float c);

  const auto& producedSound (void) const {
    return _sounds;
  }

  const auto auditionSensor (void) const {
    return _auditionSensor;
  }

  // ===========================================================================
  // == Conversion

  static b2BodyUserData* get (b2Body *b);
  static const b2BodyUserData* get (const b2Body *b);

  static FixtureData* get (b2Fixture *f);
  static const FixtureData* get (const b2Fixture *f);

  void saveBrain (const std::string &prefix) const;

  // ===========================================================================

  static Critter* clone (const Critter *c, b2Body *b);
  friend void assertEqual (const Critter &lhs, const Critter &rhs,
                           bool deepcopy);

  // ===========================================================================
  // == Static computers

  static void setEfficiencyCoeffs(float c0, float &c0Coeff,
                                  float c1, float &c1Coeff);

  // age in [0,1]
  static float efficiency (float age,
                           float c0, float c0Coeff,
                           float c1, float c1Coeff);

  static float nextGrowthStepAt (uint currentStep);

  static constexpr decimal energyForCreation (void) {
    return 2*maximalEnergyStorage(MIN_SIZE);
  }

  static constexpr decimal maximalEnergyStorage (float size) {
    assert(MIN_SIZE <= size && size <= MAX_SIZE);
//    return size * BODY_DENSITY;
    return decimal(M_PI * size * size * RADIUS * RADIUS * MIN_DENSITY);
  }

  static float computeVisionRange (float visionWidth);

  static float agingSpeed (float clockSpeed);
  static float baselineEnergyConsumption (float size, float clockSpeed);

  static float lifeExpectancy (float clockSpeed);
  static float starvationDuration (float size, float energy, float clockSpeed);

  static nlohmann::json save (const Critter &c);
  static Critter* load (const nlohmann::json &j, b2Body *body);

private:
  Critter (const Genome &g, b2Body *b);

  Color computeCurrentColor(void) const;

  // ===========================================================================
  // == Iteration substeps

  void drivingCorrections (void);
  void performVision (const Environment &env);
  void neuralStep (void);
  void energyConsumption (Environment &env);
  void regeneration (Environment &env);
  void aging (Environment &env);

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
  b2Fixture* addAuditionFixture (void);
  b2Fixture* addReproFixture (void);
  b2Fixture* addFixture (const b2FixtureDef &def,
                         const FixtureData &data);

  void delFixture (b2Fixture *f);

  // ===========================================================================
  // == Vision range managing internal methods
  void generateVisionRays (void);
  void updateVisionRays (void);

  // ===========================================================================
  // == ANN-related generation
  void buildBrain (void);

  // ===========================================================================
};

struct CID {
  const Critter &c;
  const std::string prefix;
  CID (const Critter *c, const std::string &p = "C") : CID(*c, p) {}
  CID (const Critter &c, const std::string &p = "C") : c(c), prefix(p) {}
  friend std::ostream& operator<< (std::ostream &os, const CID &cid) {
    std::ios::fmtflags os_flags (os.flags());
    os.setf(std::ios_base::hex, std::ios_base::basefield);
    os << cid.prefix << "0x" << cid.c.id();
    os.flags(os_flags);
    return os;
  }
};

} // end of namespace simu

#endif // simu_CRITTER_H
