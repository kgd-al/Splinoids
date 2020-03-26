#include "environment.h"

#include "critter.h"
#include "foodlet.h"
#include "config.h"

namespace simu {

#ifndef NDEBUG
#define IFDEBUG(X) X
#define IFDEBUGCOMMA(X) X,
#else
#define IFDEBUG(X)
#define IFDEBUGCOMMA(X)
#endif

struct CollisionMonitor : public b2ContactListener {
  static constexpr int debugFeeding = 1;
  static constexpr int debugFighting = 1;

//  using FeedingEvent = Environment::FeedingEvent;
//  Environment::FeedingEvents &fe;

//  using FightingKey = Environment::FightingKey;
//  using FightingData = Environment::FightingData;
//  Environment::FightingEvents &fi;

//  Environment::PendingDeletions &pd;

//  using FightingDrawData = Environment::FightingDrawData;

//  IFDEBUG(std::vector<FightingDrawData> &fddVector);

//  float dt;

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

  void BeginContact(b2Contact *c) override {
    if (!c->IsTouching()) return;

    b2Fixture *fA = c->GetFixtureA(), *fB = c->GetFixtureB();
    b2Body *bA = fA->GetBody(), *bB = fB->GetBody();
    const b2BodyUserData &dA = *Critter::get(bA),
                         &dB = *Critter::get(bB);
    BodyType tA = dA.type, tB = dB.type;

    if (tA == BodyType::CRITTER && tB == BodyType::CRITTER)
      registerFightStart(critter(dA), fA, critter(dB), fB);

    else if (tA == BodyType::CRITTER && tB == BodyType::PLANT)
      registerFeedStart(critter(dA), fA, foodlet(dB));

    else if (tB == BodyType::CRITTER && tA == BodyType::PLANT)
      registerFeedStart(critter(dB), fB, foodlet(dA));

    else if (tA == BodyType::CRITTER && tB == BodyType::WARP_ZONE)
      requestTeleport(critter(dA));

    else if (tB == BodyType::CRITTER && tA == BodyType::WARP_ZONE)
      requestTeleport(critter(dB));

    else
      std::cerr << "Collision between types " << uint(tA) << " & " << uint(tB)
                << std::endl;

//    else // ignore
//      utils::doThrow<std::logic_error>(
//        "Unexpected collision start between types ",
//        int(dA.type), " and ", int(dB.type));

//    c->SetEnabled(false);
  }

  void PreSolve(b2Contact *c, const b2Manifold */*oldManifold*/) override {
    b2Fixture *fA = c->GetFixtureA(), *fB = c->GetFixtureB();
    b2Body *bA = fA->GetBody(), *bB = fB->GetBody();
    const b2BodyUserData &dA = *Critter::get(bA),
                         &dB = *Critter::get(bB);
    Critter *cA = critter(dA), *cB = critter(dB);
    Foodlet *pA = foodlet(dA), *pB = foodlet(dB);

    if (cA && cB)       processFightStep(cA, fA, cB, fB);
    else if (cA && pB)  processFeedStep(cA, fA, pB);
    else if (cB && pA)  processFeedStep(cB, fB, pA);
  }

  void EndContact(b2Contact *c) override {
    b2Fixture *fA = c->GetFixtureA(), *fB = c->GetFixtureB();
    b2Body *bA = fA->GetBody(), *bB = fB->GetBody();
    const b2BodyUserData &dA = *Critter::get(bA),
                         &dB = *Critter::get(bB);
    Critter *cA = critter(dA), *cB = critter(dB);
    Foodlet *pA = foodlet(dA), *pB = foodlet(dB);

    if (cA && cB)       registerFightEnd(cA, fA, cB, fB);
    else if (cA && pB)  registerFeedEnd(cA, pB);
    else if (cB && pA)  registerFeedEnd(cB, pA);
//    else  // ignore
//      utils::doThrow<std::logic_error>(
//        "Unexpected collision end between types ",
//        int(dA.type), " and ", int(dB.type));
  }

  void registerFightStart (Critter *cA, b2Fixture *fA,
                           Critter *cB, b2Fixture *fB) {
    if (debugFighting) {
      const Critter::FixtureData &fdA = *Critter::get(fA);
      const Critter::FixtureData &fdB = *Critter::get(fB);
      std::cerr << "Fight started between " << cA->id() << " (" << fdA
                << ") and " << cB->id() << " (" << fdB << ")"
                << std::endl;
    }

    if (cA->id() > cB->id()) {  // ensure order
      std::swap(cA, cB);  std::swap(fA, fB);
    }

    Environment::FightingData &d = e._fightingEvents[std::make_pair(cA,cB)];
    d.fixtures.insert({fA,fB});
  }

  void processFightStep (Critter *cA, b2Fixture *fA,
                         Critter *cB, b2Fixture *fB) {

    if (cA->id() > cB->id()) {  // ensure order
      std::swap(cA, cB);  std::swap(fA, fB);
    }

    Environment::FightingData &d = e._fightingEvents.at(std::make_pair(cA, cB));
    d.velocities[0] = cA->body().GetLinearVelocity();
    d.velocities[1] = cB->body().GetLinearVelocity();

//    // TODO URGENT Move back to environment and process only once per critter pair

//    Environment::FightingDrawData fdd;
//    fdd.vA = cA->body().GetLinearVelocity();
//    fdd.vB = cB->body().GetLinearVelocity();

//    // Ignore small enough collisions
//    if (fdd.vA.Length() + fdd.vB.Length() < 1e-2) return;

//    const Critter::FixtureData &fdA = *Critter::get(fA);
//    const Critter::FixtureData &fdB = *Critter::get(fB);

//    fdd.pA = cA->body().GetWorldPoint(fdA.centerOfMass);
//    fdd.pB = cB->body().GetWorldPoint(fdB.centerOfMass);
//    fdd.C = fdd.pB - fdd.pA;
//    fdd.C_ = fdd.C;
//    fdd.C_.Normalize();

//    fdd.VA = b2Dot(fdd.vA,  fdd.C_);
//    fdd.VB = b2Dot(fdd.vB, -fdd.C_);

//    float alpha = (fB->GetDensity())
//        / (fA->GetDensity() + fB->GetDensity());

//    float N = config::Simulation::combatBaselineIntensity() * (
//        cA->body().GetMass() * fdd.VA * fdd.VA
//      + cB->body().GetMass() * fdd.VB * fdd.VB);

//    float deA = alpha * .5 * N,
//          deB = (1 - alpha) * .5 * N;

//    if (debugFighting > 1)
//      std::cerr << "\tFixtures C" << cA->id() << "F" << fdA << " and "
//                << "C" << cB->id() << "F" << fdB << "\n"
//                << "\t    at " << cA->body().GetWorldPoint(fdA.centerOfMass)
//                << " and " << cB->body().GetWorldPoint(fdB.centerOfMass)
//                << "\n"
//                << "\t\t C = " << fdd.C << "\n"
//                << "\t\tC' = " << fdd.C_ << "\n"
//                << "\t\tVA = " << fdd.VA << " = " << fdd.vA << "."
//                  << fdd.C_ << "\n"
//                << "\t\tVB = " << fdd.VB << " = " << fdd.vB << "."
//                  << fdd.C_ << "\n"
//                << "\t\trA = " << fA->GetDensity() << "\trB = "
//                  << fB->GetDensity() << "\n"
//                << "\t\t N = " << N << "\n"
//                << "\t\tdA = " << deA << "\n"
//                << "\t\tdB = " << deB << "\n";

//    bool aSplineDestroyed = cA->applyHealthDamage(fdA, deA, e);
//    if (aSplineDestroyed)
//      e._pendingDeletions.insert({cA, Critter::splineIndex(fdA)});
//    if (aSplineDestroyed)
//      std::cerr << "Spline " << fdA << " (" << Critter::splineIndex(fdA)
//                << ") should be destroyed\n";

//    bool bSplineDestroyed = cB->applyHealthDamage(fdB, deB, e);
//    if (bSplineDestroyed)
//      e._pendingDeletions.insert({cB, Critter::splineIndex(fdB)});

//    if (bSplineDestroyed)
//      std::cerr << "Spline " << fdB << " (" << Critter::splineIndex(fdB)
//                << ") should be destroyed\n";

//#ifndef NDEBUG
//    e.fightingDrawData.push_back(fdd);
//#endif
  }

  void registerFightEnd (Critter *cA, b2Fixture *fA,
                         Critter *cB, b2Fixture *fB) {
    if (debugFighting) {
      const Critter::FixtureData &fdA = *Critter::get(fA);
      const Critter::FixtureData &fdB = *Critter::get(fB);
      std::cerr << "Fight concluded between " << cA->id() << " (" << fdA
                << ") and " << cB->id() << " (" << fdB << ")"
                << std::endl;
    }

    if (cA->id() > cB->id()) {  // ensure order
      std::swap(cA, cB);  std::swap(fA, fB);
    }

    Environment::FightingKey k {cA,cB};
    auto it = e._fightingEvents.find(k);
    if (it != e._fightingEvents.end()) {
      it->second.erase({fA,fB});
      if (it->second.empty())
        e._fightingEvents.erase(it);
    } else
      utils::doThrow<std::logic_error>(
        "Removal of conflict ", cA->id(), "-", cB->id(),
        " requested but none were found");
  }

  void registerFeedStart (Critter *c, b2Fixture *f, Foodlet *p) {
    // If not a body
    if (f->GetShape()->GetType() != b2Shape::e_circle)  return;

    if (debugFeeding)
      std::cerr << "Feeding started by C" << c->id() << " on P" << p->id()
                << std::endl;

    e._feedingEvents.insert({c,p});
  }

  void processFeedStep (Critter *c, b2Fixture *f, Foodlet *p) {
    static const decimal &dE = config::Simulation::energyAbsorptionRate();
    if (f->GetShape()->GetType() != b2Shape::e_circle)  return;
    if (p->energy() <= 0) return;

    decimal E = dE * e.dt();
    E = std::min(E, p->energy());
    E = std::min(E, c->storableEnergy());
    c->feed(E);
    p->consumed(E);

    if (debugFeeding > 1)
      std::cerr << "Transfering " << E << " from " << p->id()
                << " to " << c->id() << " (" << p->energy()
                << " remaining)" << std::endl;
  }

  void registerFeedEnd (Critter *c, Foodlet *p) {
    if (debugFeeding)
      std::cerr << "Feeding concluded by C" << c->id() << " on P" << p->id()
                << std::endl;

    e._feedingEvents.erase({c,p});
  }

  void requestTeleport (Critter *c) {
    e._teleportRequests.insert(c);
  }
};

Environment::Environment(const Genome &g)
  : _genome(g), _physics({0,0}),
    _cmonitor(new CollisionMonitor(*this)) {

  createEdges();
  _physics.SetContactListener(_cmonitor);

  _energyReserve = 0;
}

Environment::~Environment (void) {
  _physics.DestroyBody(_edges);
  delete _cmonitor;
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
  _pendingDeletions.clear();
  _teleportRequests.clear();

  _physics.Step(float(dt()), V_ITER, P_ITER);

  for (const auto &d: _pendingDeletions)
    d.first->destroySpline(d.second);

  for (Critter *c: _teleportRequests) doTeleport(c);

//  std::cerr << "Physical step took " << _physics.GetProfile().step
//            << " ms" << std::endl;
}

void Environment::createEdges(void) {
  real HS = extent();
  P2D edgesVertices [4] {
    { HS, HS }, { -HS, HS }, { -HS, -HS }, { HS, -HS }
  };

  b2BodyDef edgesBodyDef;
  edgesBodyDef.type = b2_staticBody;
  edgesBodyDef.position.Set(0, 0);

  _edges = _physics.CreateBody(&edgesBodyDef);

  b2ChainShape edgesShape;
  edgesShape.CreateLoop(edgesVertices, 4);

  b2FixtureDef edgesFixture;
  edgesFixture.shape = &edgesShape;
  edgesFixture.density = 0;
  edgesFixture.isSensor = isTaurus();

  _edges->CreateFixture(&edgesFixture);

  _edgesUserData.type = isTaurus() ? BodyType::WARP_ZONE : BodyType::OBSTACLE;
  _edgesUserData.ptr.obstacle = nullptr;
  _edges->SetUserData(&_edgesUserData);
}

void Environment::processFight(void) {
  // TODO URGENT Move back to environment and process only once per critter pair

  FightingDrawData fdd;
  fdd.vA = cA->body().GetLinearVelocity();
  fdd.vB = cB->body().GetLinearVelocity();

  // Ignore small enough collisions
  if (fdd.vA.Length() + fdd.vB.Length() < 1e-2) return;

  const Critter::FixtureData &fdA = *Critter::get(fA);
  const Critter::FixtureData &fdB = *Critter::get(fB);

  fdd.pA = cA->body().GetWorldPoint(fdA.centerOfMass);
  fdd.pB = cB->body().GetWorldPoint(fdB.centerOfMass);
  fdd.C = fdd.pB - fdd.pA;
  fdd.C_ = fdd.C;
  fdd.C_.Normalize();

  fdd.VA = b2Dot(fdd.vA,  fdd.C_);
  fdd.VB = b2Dot(fdd.vB, -fdd.C_);

  float alpha = (fB->GetDensity())
      / (fA->GetDensity() + fB->GetDensity());

  float N = config::Simulation::combatBaselineIntensity() * (
      cA->body().GetMass() * fdd.VA * fdd.VA
    + cB->body().GetMass() * fdd.VB * fdd.VB);

  float deA = alpha * .5 * N,
        deB = (1 - alpha) * .5 * N;

  if (debugFighting > 1)
    std::cerr << "\tFixtures C" << cA->id() << "F" << fdA << " and "
              << "C" << cB->id() << "F" << fdB << "\n"
              << "\t    at " << cA->body().GetWorldPoint(fdA.centerOfMass)
              << " and " << cB->body().GetWorldPoint(fdB.centerOfMass)
              << "\n"
              << "\t\t C = " << fdd.C << "\n"
              << "\t\tC' = " << fdd.C_ << "\n"
              << "\t\tVA = " << fdd.VA << " = " << fdd.vA << "."
                << fdd.C_ << "\n"
              << "\t\tVB = " << fdd.VB << " = " << fdd.vB << "."
                << fdd.C_ << "\n"
              << "\t\trA = " << fA->GetDensity() << "\trB = "
                << fB->GetDensity() << "\n"
              << "\t\t N = " << N << "\n"
              << "\t\tdA = " << deA << "\n"
              << "\t\tdB = " << deB << "\n";

  bool aSplineDestroyed = cA->applyHealthDamage(fdA, deA, e);
  if (aSplineDestroyed)
    e._pendingDeletions.insert({cA, Critter::splineIndex(fdA)});
  if (aSplineDestroyed)
    std::cerr << "Spline " << fdA << " (" << Critter::splineIndex(fdA)
              << ") should be destroyed\n";

  bool bSplineDestroyed = cB->applyHealthDamage(fdB, deB, e);
  if (bSplineDestroyed)
    e._pendingDeletions.insert({cB, Critter::splineIndex(fdB)});

  if (bSplineDestroyed)
    std::cerr << "Spline " << fdB << " (" << Critter::splineIndex(fdB)
              << ") should be destroyed\n";

#ifndef NDEBUG
  e.fightingDrawData.push_back(fdd);
#endif
}

void Environment::doTeleport(Critter *c) {
  b2Body &b = c->body();
  auto a = b.GetAngle();
  P2D p0 = b.GetPosition(), p1 = p0;

  if (p1.x < -extent())      p1.x += size();
  else if (p1.x > extent())  p1.x -= size();

  if (p1.y < -extent())      p1.y += size();
  else if (p1.y > extent())  p1.y -= size();

  if (p0 != p1) b.SetTransform(p1, a);

  if (p0 != p1)
    std::cerr << "Teleporting C" << c->id() << " from " << p0 << " to "
              << p1 << std::endl;
}

decimal Environment::dt (void) {
  static const decimal DT = 1.f / config::Simulation::secondsPerDay();
  return DT;
}

} // end of namespace simu
