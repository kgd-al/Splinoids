#include "environment.h"

#include "critter.h"
#include "foodlet.h"
#include "config.h"
#include "box2dutils.h"

namespace simu {

static constexpr int debugFeeding = 0;
static constexpr int debugHearing = 0;
static constexpr int debugMating = 0;
static constexpr int debugFighting = 0;
static constexpr int debugTouching = 0;

// between arms of a single critter
static constexpr bool ignoreSelfCollisions = true;

const bool Environment::boxEdges = true;

struct UDID {
  const b2Body *b;
  const b2BodyUserData &d;
  const b2Fixture *f;
  UDID(const b2Body *b, const b2BodyUserData &d, const b2Fixture *f)
    : b(b), d(d), f(f) {}
  friend std::ostream& operator<< (std::ostream &os, const UDID &udid) {
    switch (udid.d.type) {
    case BodyType::CRITTER:
      return os << CID(udid.d.ptr.critter) << "F" << *Critter::get(udid.f);
    case BodyType::CORPSE:
    case BodyType::PLANT:
    case BodyType::OBSTACLE:
    case BodyType::WARP_ZONE:
    default:
      return os << "[" << uint(udid.d.type) << "@" << udid.b << "]";
    }
  }
};

struct CollisionMonitor : public b2ContactListener {
  Environment &e;

  CollisionMonitor (Environment &e) : e(e) {}

  Critter* critter (const b2BodyUserData &d) {
    if (d.type == BodyType::CRITTER)  return d.ptr.critter;
    return nullptr;
  }

  Foodlet* foodlet (const b2BodyUserData &d) {
    if (d.type == BodyType::PLANT
        || d.type == BodyType::CORPSE)  return d.ptr.foodlet;
    return nullptr;
  }

  static constexpr uint16 pair (const BodyType &lhs, const BodyType &rhs) {
    static_assert(sizeof(BodyType) == 1,
        "BodyType is using a larger underlying type than expected");
    static_assert(sizeof(BodyType) * 2 == sizeof(uint16),
        "Something went wrong in my computations...");
    return (uint16(lhs) << 8) | uint16(rhs);
  }

  Critter* isTouch (const b2BodyUserData &dA, b2Fixture *fA,
                    const b2BodyUserData &dB, b2Fixture *fB) {
    if (fB->IsSensor()) return nullptr;

    auto cA = critter(dA);
    if (!cA) return nullptr;

    if (critter(dB) == cA)  return nullptr;

    switch (Critter::get(fA)->type) {
    case Critter::FixtureType::BODY:
    case Critter::FixtureType::ARTIFACT:
      return cA;
    case Critter::FixtureType::AUDITION:
    case Critter::FixtureType::REPRODUCTION:
      return nullptr;
    }

    return nullptr;
  }

  bool isAudition (const b2Fixture *fA, const b2Fixture *fB) {
    /// TODO maybe dangerous
    return (Critter::get(fA)->type | Critter::get(fB)->type)
        == (Critter::FixtureType::AUDITION | Critter::FixtureType::BODY);
  }

  bool isMatingAttempt (const b2Fixture *fA, const b2Fixture *fB) {
    return (Critter::get(fA)->type & Critter::get(fB)->type)
        == Critter::FixtureType::REPRODUCTION;
  }

  struct Velocities { float A, B; };
  Velocities velocities (const b2Contact *c, b2Fixture *fA, b2Fixture *fB) {
    b2WorldManifold m;
    c->GetWorldManifold(&m);
    static const auto velocity = [] (const b2Body &b, const b2Vec2 &p) {
      return b.GetLinearVelocityFromWorldPoint(p).Length();
    };
    return { velocity(Critter::get(fA)->body, m.points[0]),
             velocity(Critter::get(fB)->body, m.points[0]) };
  }

  void BeginContact(b2Contact *c) override {
    if (!c->IsTouching()) return;

    b2Fixture *fA = c->GetFixtureA(), *fB = c->GetFixtureB();
    b2Body *bA = fA->GetBody(), *bB = fB->GetBody();
    const b2BodyUserData &dA = *Critter::get(bA),
                         &dB = *Critter::get(bB);

    if (auto c = isTouch(dA, fA, dB, fB)) registerTouchStart(c, fA);
    if (auto c = isTouch(dB, fB, dA, fA)) registerTouchStart(c, fB);

    switch (pair(dA.type, dB.type)) {
    case pair(BodyType::CRITTER, BodyType::CRITTER):
      if (isAudition(fA, fB))
        registerStartOfHearing(critter(dA), critter(dB));
      else if (isMatingAttempt(fA, fB))
        registerStartOfMatingAttempt(critter(dA), critter(dB));
      else
        registerFightStart(critter(dA), fA, critter(dB), fB);
      break;

    case pair(BodyType::CRITTER, BodyType::PLANT):
    case pair(BodyType::CRITTER, BodyType::CORPSE):
        registerFeedStart(critter(dA), fA, foodlet(dB));  break;

    case pair(BodyType::PLANT, BodyType::CRITTER):
    case pair(BodyType::CORPSE, BodyType::CRITTER):
        registerFeedStart(critter(dB), fB, foodlet(dA));  break;

    case pair(BodyType::CRITTER, BodyType::WARP_ZONE):
        watchForWarp(critter(dA), Critter::get(fA));  break;

    case pair(BodyType::WARP_ZONE, BodyType::CRITTER):
        watchForWarp(critter(dB), Critter::get(fB));  break;

    case pair(BodyType::OBSTACLE, BodyType::CRITTER):
    case pair(BodyType::CRITTER, BodyType::OBSTACLE):
      break;

    default:
        std::cerr << "Collision started between types " << uint(dA.type)
                  << " & " << uint(dB.type) << std::endl;
      break;
    }
  }

  void PreSolve(b2Contact *c, const b2Manifold */*oldManifold*/) override {
    b2Fixture *fA = c->GetFixtureA(), *fB = c->GetFixtureB();
    b2Body *bA = fA->GetBody(), *bB = fB->GetBody();
    const b2BodyUserData &dA = *Critter::get(bA),
                         &dB = *Critter::get(bB);

    // Disable collisions between moving parts of a critter
    if (dA.type == BodyType::CRITTER && dB.type == BodyType::CRITTER
        && dA.ptr.critter == dB.ptr.critter) {
      if (ignoreSelfCollisions) c->SetEnabled(false);
      return;
    }

//    std::cerr << __PRETTY_FUNCTION__ << "\n"
//              << UDID(bA, dA, fA) << " & " << UDID(bB, dB, fB) << "\n";
//    b2WorldManifold m;
//    c->GetWorldManifold(&m);
//    for (int i=0; i<c->GetManifold()->pointCount; i++)
//      std::cerr << m.points[i] << "\n";
//    for (b2Body *b: {bA, bB}) b->Dump();
//    std::cerr << "\n";

    switch (pair(dA.type, dB.type)) {
    case pair(BodyType::CRITTER, BodyType::CRITTER):
      if (!isMatingAttempt(fA, fB))
        processFightPreStep(c, critter(dA), fA, critter(dB), fB);
      break;

    case pair(BodyType::CRITTER, BodyType::PLANT):
    case pair(BodyType::CRITTER, BodyType::CORPSE):
      processFeedStep(critter(dA), fA, foodlet(dB));  break;

    case pair(BodyType::PLANT, BodyType::CRITTER):
    case pair(BodyType::CORPSE, BodyType::CRITTER):
      processFeedStep(critter(dB), fB, foodlet(dA));  break;

    default:
//      std::cerr << "Collision step between types " << uint(tA) << " & "
//                << uint(tB) << std::endl;
      break;
    }
  }

  void PostSolve(b2Contact* c, const b2ContactImpulse* impulse) {
    b2Fixture *fA = c->GetFixtureA(), *fB = c->GetFixtureB();
    b2Body *bA = fA->GetBody(), *bB = fB->GetBody();
    const b2BodyUserData &dA = *Critter::get(bA),
                         &dB = *Critter::get(bB);

//    std::cerr << __PRETTY_FUNCTION__ << "\n" << std::setprecision(20)
//              << UDID(bA, dA, fA) << " & " << UDID(bB, dB, fB) << "\n"
//              << "Impulse:\n";
//    for (uint i=0; i<impulse->count; i++)
//      std::cerr << "\t" << impulse->normalImpulses[i]
//                << "\t" << impulse->tangentImpulses[i]
//                << "\n";

    switch (pair(dA.type, dB.type)) {
    case pair(BodyType::CRITTER, BodyType::CRITTER):
      if (!isMatingAttempt(fA, fB))
        processFightPostStep(c, critter(dA), fA, critter(dB), fB, impulse);
      break;

    default:
//      std::cerr << "Collision step between types " << uint(tA) << " & "
//                << uint(tB) << std::endl;
      break;
    }
  }

  void EndContact(b2Contact *c) override {
    b2Fixture *fA = c->GetFixtureA(), *fB = c->GetFixtureB();
    b2Body *bA = fA->GetBody(), *bB = fB->GetBody();
    const b2BodyUserData &dA = *Critter::get(bA),
                         &dB = *Critter::get(bB);

    if (auto c = isTouch(dA, fA, dB, fB)) registerTouchEnd(c, fA);
    if (auto c = isTouch(dB, fB, dA, fA)) registerTouchEnd(c, fB);

    switch (pair(dA.type, dB.type)) {
    case pair(BodyType::CRITTER, BodyType::CRITTER):
      if (isAudition(fA, fB))
        registerEndOfHearing(critter(dA), critter(dB));
      else if (isMatingAttempt(fA, fB))
        registerEndOfMatingAttempt(critter(dA), critter(dB));
      else
        registerFightEnd(critter(dA), fA, critter(dB), fB);
      break;

    case pair(BodyType::CRITTER, BodyType::PLANT):
    case pair(BodyType::CRITTER, BodyType::CORPSE):
      registerFeedEnd(critter(dA), foodlet(dB));  break;

    case pair(BodyType::PLANT, BodyType::CRITTER):
    case pair(BodyType::CORPSE, BodyType::CRITTER):
      registerFeedEnd(critter(dB), foodlet(dA));  break;

    case pair(BodyType::CRITTER, BodyType::WARP_ZONE):
      unwatchForWarp(critter(dA), Critter::get(fA));  break;

    case pair(BodyType::WARP_ZONE, BodyType::CRITTER):
      unwatchForWarp(critter(dB), Critter::get(fB));  break;

    case pair(BodyType::OBSTACLE, BodyType::CRITTER):
    case pair(BodyType::CRITTER, BodyType::OBSTACLE):
      break;

    default:
//      std::cerr << "Collision finished between types " << uint(tA) << " & "
//                << uint(tB) << std::endl;
      break;
    }
  }

  // ===========================================================================

  void registerTouchStart (Critter *c, b2Fixture *f) {
    Environment::CritterData &d = e._critterData[c];
    d.collisions++;
    c->registerContact(*Critter::get(f), true);

    if (debugTouching)
      std::cerr << "Start of touch event for " << CID(c) << "F"
                << *Critter::get(f) << "\n";
  }

  void registerTouchEnd (Critter *c, b2Fixture *f) {
    auto it = e._critterData.find(c);
    if (it == e._critterData.end())
      utils::doThrow<std::logic_error>(
        "Removal of data for critter ", CID(c),
        " requested but none were found");

    Environment::CritterData &d = it->second;
    d.collisions--;
    c->registerContact(*Critter::get(f), false);

    if (debugTouching)
      std::cerr << "End of touch event for " << CID(c) << "F"
                << *Critter::get(f) << "\n";

    if (d.collisions == 0)  e._critterData.erase(it);
  }

  // ===========================================================================

  void registerFightStart (Critter *cA, b2Fixture *fA,
                           Critter *cB, b2Fixture *fB) {

    if (cA == cB) return; // Different moving parts of the same critter

    if (cA->id() > cB->id()) {  // ensure order
      std::swap(cA, cB);  std::swap(fA, fB);
    }

    if (debugFighting) {
      const Critter::FixtureData &fdA = *Critter::get(fA);
      const Critter::FixtureData &fdB = *Critter::get(fB);
      std::cerr << "Fight started between " << CID(cA) << " (" << fdA
                << ") and " << CID(cB) << " (" << fdB << ")"
                << std::endl;
    }

    Environment::FightingData &d = e._fightingEvents[std::make_pair(cA,cB)];
    d.fixtures.insert({{fA,fB}, Environment::FightingData::FixturesData{}});
  }

  void processFightPreStep (b2Contact *contact,
                            Critter *cA, b2Fixture *fA,
                            Critter *cB, b2Fixture *fB) {
    if (cA == cB) return; // Different moving parts of the same critter

    if (cA->id() > cB->id()) { // ensure order
      std::swap(cA, cB); std::swap(fA, fB);
    }

    for (Critter *c: { cA, cB }) {
      Environment::CritterData &d = e._critterData.at(c);
      d.totalImpulsions = 0;
    }

    Environment::FightingData &d = e._fightingEvents.at({ cA, cB });
    auto &fd = d.fixtures.at({fA,fB});
    auto v = velocities(contact, fA, fB);
    fd.A.velocity[0] = v.A;
    fd.B.velocity[0] = v.B;
  }

  void processFightPostStep (b2Contact *contact,
                             Critter *cA, b2Fixture *fA,
                             Critter *cB, b2Fixture *fB,
                             const b2ContactImpulse *impulse) {
    if (cA == cB) return; // Different moving parts of the same critter

    if (cA->id() > cB->id()) {  // ensure order
      std::swap(cA, cB);  std::swap(fA, fB);
    }

    float totalImpulse = 0;
    for (int i=0; i<impulse->count; i++)
      totalImpulse += impulse->normalImpulses[i];

    for (Critter *c: { cA, cB })
      e._critterData.at(c).totalImpulsions += totalImpulse;

    Environment::FightingData &d = e._fightingEvents.at({ cA, cB });
    auto &fd = d.fixtures.at({fA,fB});
    fd.impulse = totalImpulse;

    auto v = velocities(contact, fA, fB);
    fd.A.velocity[1] = v.A;
    fd.B.velocity[1] = v.B;
  }

  void registerFightEnd (Critter *cA, b2Fixture *fA,
                         Critter *cB, b2Fixture *fB) {
    if (cA == cB) return; // Different moving parts of the same critter

    if (cA->id() > cB->id()) {  // ensure order
      std::swap(cA, cB);  std::swap(fA, fB);
    }

    if (debugFighting) {
      const Critter::FixtureData &fdA = *Critter::get(fA);
      const Critter::FixtureData &fdB = *Critter::get(fB);
      std::cerr << "Fight concluded between " << CID(cA) << " (" << fdA
                << ") and " << CID(cB) << " (" << fdB << ")"
                << std::endl;
    }

    auto it = e._fightingEvents.find({cA,cB});
    if (it == e._fightingEvents.end())
      utils::doThrow<std::logic_error>(
        "Removal of conflict ", CID(cA), "-", CID(cB),
        " requested but none were found");

    Environment::FightingData &d = it->second;
    d.fixtures.erase({fA,fB});
    if (d.fixtures.empty()) e._fightingEvents.erase(it);
  }

  void registerFeedStart (Critter *c, b2Fixture *f, Foodlet *p) {
    // If not a body
    if (f->GetShape()->GetType() != b2Shape::e_circle)  return;

    if (debugFeeding)
      std::cerr << "Feeding started by " << CID(c) << " on P" << p->id()
                << std::endl;

    e._feedingEvents.insert({c,p});
  }

  void processFeedStep (Critter *c, b2Fixture *f, Foodlet *p) {
    if (f->GetShape()->GetType() != b2Shape::e_circle)  return;
    if (c->storableEnergy() <= 0) return;
    if (p->energy() <= 0) return;

    c->feed(p, e.dt());
  }

  void registerFeedEnd (Critter *c, Foodlet *p) {
    if (debugFeeding)
      std::cerr << "Feeding concluded by " << CID(c) << " on P" << p->id()
                << std::endl;

    e._feedingEvents.erase({c,p});
  }

  void registerStartOfHearing (Critter *cA, Critter *cB) {
    /// TODO Find a way (latter) to prevent both contacts to register
    if (debugHearing/* && cA->id() < cB->id()*/)
      std::cerr << "Start of hearing between " << CID(cA) << " & " << CID(cB)
                << std::endl;
    if (cA->id() < cB->id())
      e._hearingEvents.insert({cA,cB});
  }

  void registerEndOfHearing (Critter *cA, Critter *cB) {
    if (debugHearing/* && cA->id() < cB->id()*/)
      std::cerr << "End of hearing between " << CID(cA) << " & " << CID(cB)
                << std::endl;
    if (cA->id() < cB->id())
      e._hearingEvents.erase({cA,cB});
  }

  void registerStartOfMatingAttempt (Critter *cA, Critter *cB) {
    static constexpr auto S = Critter::Genome::SEXUAL;
    if (cA->sex() == cB->sex()) return;
    assert(cA->hasSexualReproduction());
    assert(cB->hasSexualReproduction());

    if (debugMating)
      std::cerr << "Start of mating attempt between " << CID(cA) << " ("
                << cA->requestingMating(S) << ": "
                << cA->reproductionReadiness(S) << ", "
                << cA->reproductionOutput() << ") & " << CID(cB) << " ("
                << cB->requestingMating(S) << ": "
                << cB->reproductionReadiness(S)
                << ", " << cB->reproductionOutput() << ")" << std::endl;
    e._matingEvents.insert({cA,cB});
  }

  void registerEndOfMatingAttempt (Critter *cA, Critter *cB) {
    if (cA->sex() == cB->sex()) return;

    if (debugMating)
      std::cerr << "End of mating attempt between " << CID(cA) << " & "
                << CID(cB) << std::endl;
    e._matingEvents.erase({cA,cB});
  }

  void watchForWarp (Critter *c, const Critter::FixtureData *fd) {
//    std::cerr << "Maybe teleport " << CID(c) << " (" << *fd << ") which is at "
//              << c->pos() << std::endl;
    if (fd->type == Critter::FixtureType::BODY) e._edgeCritters.insert(c);
  }

  void unwatchForWarp (Critter *c, const Critter::FixtureData *fd) {
//    std::cerr << "Maybe no longer teleport " << CID(c) << " (" << *fd
//              << ") which is at " << c->pos() << std::endl;
    if (fd->type == Critter::FixtureType::BODY) e._edgeCritters.erase(c);
  }
};

bool Environment::ID_CMP::operator() (const DestroyedSpline &lhs,
                                      const DestroyedSpline &rhs) const {
  if (lhs.first->id() != rhs.first->id())
    return lhs.first->id() < rhs.first->id();
  return lhs.second < rhs.second;
}


Environment::Environment(const Genome &g)
  : _genome(g), _physics({0,0}),
    _cmonitor(new CollisionMonitor(*this)) {

  createEdges();
  _physics.SetContactListener(_cmonitor);

  _energyReserve = 0;

  _obstaclesUserData.type = BodyType::OBSTACLE;
  _obstaclesUserData.ptr.obstacle = nullptr;
}

Environment::~Environment (void) {
  _physics.DestroyBody(_edges);
  delete _cmonitor;
}

void Environment::init(decimal energy, uint rngSeed) {
  _energyReserve = energy;
  if (rngSeed != uint(-1)) _dice.reset(rngSeed);
}

void Environment::modifyEnergyReserve (decimal e) {
//  std::cerr << "Environment received " << (e>0?"+":"") << e << ", reserves are "
//            << _energyReserve;
  _energyReserve += e;
//  std::cerr << " >> " << _energyReserve << std::endl;
}

void Environment::step (void) {

  // Box2D parameters
  static const int V_ITER = config::Simulation::b2VelocityIter();
  static const int P_ITER = config::Simulation::b2PositionIter();

#ifndef NDEBUG
  fightingDrawData.clear();
#endif

  _physics.Step(float(dt()), V_ITER, P_ITER);

  if (debugFighting && !_fightingEvents.empty())
    std::cerr << ">> Processing fight events\n";

  DestroyedSplines destroyedSplines;
  for (const auto &f: _fightingEvents)
    processFight(f.first.first, f.first.second, f.second, destroyedSplines);
  for (const auto &p: destroyedSplines)
    p.first->destroySpline(p.second);

  for (Critter *c: _edgeCritters) maybeTeleport(c);

//  std::cerr << "Physical step took " << _physics.GetProfile().step
//            << " ms" << std::endl;
}

b2Body* Environment::createObstacle(float x, float y, float w, float h) {
  b2BodyDef bodyDef;
  bodyDef.type = b2_staticBody;
  bodyDef.position.Set(x+.5f*w, y+.5f*h);

  b2Body *body = _physics.CreateBody(&bodyDef);

  b2PolygonShape box;
  box.SetAsBox(.5f*w, .5f*h);

  b2FixtureDef fixtureDef;
  fixtureDef.shape = &box;
  fixtureDef.density = 0;
  fixtureDef.isSensor = false;
  fixtureDef.filter.categoryBits = uint16(CollisionFlag::OBSTACLE_FLAG);
  fixtureDef.filter.maskBits = uint16(CollisionFlag::OBSTACLE_MASK);

  body->CreateFixture(&fixtureDef);
  body->SetUserData(&_obstaclesUserData);

  return body;
}

void Environment::createEdges(void) {
  static constexpr float W = 10, W2 = 2*W;
  real HW = xextent(), HH = yextent();

  b2BodyDef edgesBodyDef;
  edgesBodyDef.type = b2_staticBody;
  edgesBodyDef.position.Set(0, 0);

  _edges = _physics.CreateBody(&edgesBodyDef);

  if (!boxEdges) { // Linear edges
    P2D edgesVertices [4] {
      { HW, HH }, { -HW, HH }, { -HW, -HH }, { HW, -HH }
    };

    b2ChainShape edgesShape;
    edgesShape.CreateLoop(edgesVertices, 4);

    b2FixtureDef edgesFixture;
    edgesFixture.shape = &edgesShape;
    edgesFixture.density = 0;
    edgesFixture.isSensor = isTaurus();

    _edges->CreateFixture(&edgesFixture);

  } else {  // Box edges
    b2PolygonShape boxes [4];
    boxes[0].SetAsBox(W, HH+W2, {  HW+W,     0}, 0);
    boxes[1].SetAsBox(HW+W2, W, {     0,  HH+W}, 0);
    boxes[2].SetAsBox(W, HH+W2, { -HW-W,     0}, 0);
    boxes[3].SetAsBox(HW+W2, W, {     0, -HH-W}, 0);

    for (b2PolygonShape &s: boxes) {
      b2FixtureDef edgeDef;
      edgeDef.shape = &s;
      edgeDef.density = 0;
      edgeDef.isSensor = isTaurus();
      edgeDef.filter.categoryBits = uint16(CollisionFlag::OBSTACLE_FLAG);
      edgeDef.filter.maskBits = uint16(CollisionFlag::OBSTACLE_MASK);

      _edges->CreateFixture(&edgeDef);
    }
  }

  _edgesUserData.type = isTaurus() ? BodyType::WARP_ZONE : BodyType::OBSTACLE;
  _edgesUserData.ptr.obstacle = nullptr;
  _edges->SetUserData(&_edgesUserData);
}

auto densityCollisionFactor (float d) {
  static constexpr float MIN_DENSITY_ALPHA = .25;
  static constexpr float MAX_DENSITY_ALPHA = 1;
  static constexpr float DENSITY_ALPHA_RANGE =
      MAX_DENSITY_ALPHA - MIN_DENSITY_ALPHA;
  static constexpr float DENSITY_RANGE =
      Critter::MAX_DENSITY - Critter::MIN_DENSITY;

  return MIN_DENSITY_ALPHA + DENSITY_ALPHA_RANGE *
      (d - Critter::MIN_DENSITY) / DENSITY_RANGE;
}

void Environment::processFight(Critter *cA, Critter *cB, const FightingData &d,
                               DestroyedSplines &ds) {

  static const auto &CMI = config::Simulation::combatMinImpulse();
  static const auto &CMV = config::Simulation::combatMinVelocity();
  static const auto &CBI = config::Simulation::combatBaselineIntensity();

  static const auto getDV = [] (auto v) { return v[0] - v[1]; };

  static const auto skip = [] (auto p) {
    return p.second.impulse < CMI
        || getDV(p.second.A.velocity) < CMV
        || getDV(p.second.B.velocity) < CMV;
  };

  const CritterData &dA = _critterData.at(cA),
                    &dB = _critterData.at(cB);

  bool ignoreA = dA.totalImpulsions < CMV,
       ignoreB = dB.totalImpulsions < CMV;

  if (ignoreA && ignoreB) return;

  if (debugFighting > 1)
    std::cerr << "\nFight step of " << CID(cA) << " & " << CID(cB) << "\n"
              << "\tat " << cA->pos() << " & " << cB->pos() << "\n";

  float fixtureCount = 0;
  for (const auto &p: d.fixtures) if (!skip(p)) fixtureCount++;

  if (debugFighting > 2 && fixtureCount > 0)
    std::cerr << "Fixture-Fixture collisions details:\n";

  for (const auto &p: d.fixtures) {
    if (skip(p)) continue;

    float impulse = p.second.impulse;
    const auto &VA = p.second.A.velocity, &VB = p.second.B.velocity;

    b2Fixture *fA = p.first.first, *fB = p.first.second;
    const Critter::FixtureData &fdA = *Critter::get(fA);
    const Critter::FixtureData &fdB = *Critter::get(fB);

    float alpha_a = densityCollisionFactor(fA->GetDensity());
    float alpha_b = densityCollisionFactor(fB->GetDensity());

    if (alpha_a == 0 && alpha_b == 0) continue;
    float alpha = (alpha_b) / (alpha_a + alpha_b);

    float dVA = std::max(0.f, getDV(VA)),
          dVB = std::max(0.f, getDV(VB)),
          dV = dVA*dVA + dVB*dVB;
    float D = CBI * dV * impulse / fixtureCount;

    float hA = cA->currentHealth(fdA), hB = cB->currentHealth(fdB);
    float D_ = std::min(D, std::min(hA / alpha, hB / (1-alpha)));

    float deA = alpha * D_, deB = (1 - alpha) * D_;

    if (debugFighting > 2) {
      std::cerr << "\tFixtures " << CID(cA) << "F" << fdA << " and "
                << CID(cB) << "F" << fdB << "\n"
                << "\t\tdVA = " << VA[0] - VA[1] << " = "
                  << VA[0] << " - " << VA[1] << "\n"
                << "\t\tdVB = " << VB[0] - VB[1] << " = "
                  << VB[0] << " - " << VB[1] << "\n"
                << "\t\trA = " << fA->GetDensity()
                  << "\t" << alpha_a << "\t" << alpha << "\n"
                << "\t\trB = " << fB->GetDensity()
                  << "\t" << alpha_b << "\t" << 1-alpha << "\n"
                << "\t\t D = " << D << " = "
                  << CBI << " * " << dV << " * " << impulse
                  << " / " << fixtureCount << "\n";
      if (D != D_)
        std::cerr << "\t\tD' = " << D_ << " (clamped to hA = " << hA << ", "
                  << "hB = " << hB << ")\n";
    }

    bool sADestroyed = cA->applyHealthDamage(fdA, deA, *this);
    if (sADestroyed)  ds.insert({cA, Critter::splineIndex(fdA)});

    bool sBDestroyed = cB->applyHealthDamage(fdB, deB, *this);
    if (sBDestroyed)  ds.insert({cB, Critter::splineIndex(fdB)});

    if (debugFighting > 2) {
      std::cerr << "\t\tdA = " << deA;
      if (sADestroyed)  std::cerr << "\t>> destroyed";
      std::cerr << "\n\t\tdB = " << deB;
      if (sBDestroyed)  std::cerr << "\t>> destroyed";
      std::cerr << "\n";
    }

    float sum_ = deA+deB, diff_ = D_ - sum_;
    assert(std::fabs(deA + deB - D_) < 1e-3);

//  #ifndef NDEBUG
//    fightingDrawData.push_back(fdd);
//  #endif
  }
}

void Environment::maybeTeleport(Critter *c) {
  b2Body &b = c->body();
  auto a = b.GetAngle();
  P2D p0 = b.GetPosition(), p1 = p0;

//  std::cerr << CID(c, "Critter ") << " touching edges at " << p0 << std::endl;

  if (p1.x < -xextent())      p1.x += width();
  else if (p1.x > xextent())  p1.x -= width();

  if (p1.y < -yextent())      p1.y += height();
  else if (p1.y > yextent())  p1.y -= height();

  if (p0 != p1) b.SetTransform(p1, a);

//  if (p0 != p1)
//    std::cerr << "Teleporting " << CID(c) << " from " << p0 << " to "
//              << p1 << std::endl;
}

void Environment::mutateController (rng::AbstractDice &dice, float r) {
  using T = rng::AbstractDice::Seed_t;
  _dice.reset(dice(T(0), std::numeric_limits<T>::max()));

//  using Config = genotype::Environment::config_t;
//  static const auto &bounds = Config::maxVegetalPortionBounds();
//  using MO = std::remove_cv_t<std::remove_reference_t<decltype(bounds)>>::Operators;

  const float max = .2, mmax = .025;
  const float min = .0;
//  float er = (1 - r) * (max - min) + min;
//  _genome.maxVegetalPortion = std::min(_genome.maxVegetalPortion, er);
//  MO::mutate(_genome.maxVegetalPortion, bounds.min, bounds.max, dice);

  static const float inertia = 0;
  _genome.maxVegetalPortion = (1-inertia) * dice(0.f, (1-r)*(max-mmax)+mmax)
                            + inertia * _genome.maxVegetalPortion;
}

decimal Environment::dt (void) {
  static const decimal DT = 1.f / config::Simulation::ticksPerSecond();
  return DT;
}

void save (nlohmann::json &j, const rng::FastDice &d) {
  std::ostringstream oss;
  serialize(oss, d);
  j = oss.str();
}

void load (const nlohmann::json &j, rng::FastDice &d) {
  std::istringstream iss (j.get<std::string>());
  deserialize(iss, d);
}

Environment *Environment::clone(const Environment &e) {
  Environment *this_e = new Environment(e._genome);

  assert(this_e->_feedingEvents.empty());
  assert(this_e->_fightingEvents.empty());
  assert(this_e->_matingEvents.empty());
  assert(this_e->_edgeCritters.empty());

  this_e->_energyReserve = e._energyReserve;

  this_e->_dice = e._dice;

  return this_e;
}

void assertEqual (const Environment &lhs, const Environment &rhs,
                  bool deepcopy) {
  using utils::assertEqual;

#define ASRT(X) assertEqual(lhs.X, rhs.X, deepcopy)
  ASRT(_genome);
//  ASRT(_physics);
//  ASRT(_cmonitor);
  ASRT(_edges);
  ASRT(_edgesUserData);
//  ASRT(_feedingEvents);
//  ASRT(_fightingEvents);
//  ASRT(_matingEvents);
//  ASRT(_edgeCritters);
  ASRT(_energyReserve);
  ASRT(_dice);
#undef ASRT
}

void Environment::save (nlohmann::json &j, const Environment &e) {
  nlohmann::json jd;
  simu::save(jd, e._dice);
  j = { e._genome, e._energyReserve, jd };
}

void Environment::load (const nlohmann::json &j,
                        std::unique_ptr<Environment> &e) {

  e.reset(new Environment(j[0]));
  e->_energyReserve = j[1];
  simu::load(j[2], e->_dice);
}

} // end of namespace simu
