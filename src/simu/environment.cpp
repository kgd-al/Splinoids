#include "environment.h"

#include "critter.h"
#include "foodlet.h"
#include "config.h"

namespace simu {

static constexpr int debugFeeding = 0;
static constexpr int debugFighting = 0;

const bool Environment::boxEdges = true;

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

  void BeginContact(b2Contact *c) override {
    if (!c->IsTouching()) return;

    b2Fixture *fA = c->GetFixtureA(), *fB = c->GetFixtureB();
    b2Body *bA = fA->GetBody(), *bB = fB->GetBody();
    const b2BodyUserData &dA = *Critter::get(bA),
                         &dB = *Critter::get(bB);

    switch (pair(dA.type, dB.type)) {
    case pair(BodyType::CRITTER, BodyType::CRITTER):
      registerFightStart(critter(dA), fA, critter(dB), fB); break;

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

    switch (pair(dA.type, dB.type)) {
    case pair(BodyType::CRITTER, BodyType::CRITTER):
      processFightStep(critter(dA), fA, critter(dB), fB); break;

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

  void EndContact(b2Contact *c) override {
    b2Fixture *fA = c->GetFixtureA(), *fB = c->GetFixtureB();
    b2Body *bA = fA->GetBody(), *bB = fB->GetBody();
    const b2BodyUserData &dA = *Critter::get(bA),
                         &dB = *Critter::get(bB);

    switch (pair(dA.type, dB.type)) {
    case pair(BodyType::CRITTER, BodyType::CRITTER):
      registerFightEnd(critter(dA), fA, critter(dB), fB); break;

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

    default:
//      std::cerr << "Collision finished between types " << uint(tA) << " & "
//                << uint(tB) << std::endl;
      break;
    }
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
      it->second.fixtures.erase({fA,fB});
      if (it->second.fixtures.empty())
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

    decimal E = dE * c->clockSpeed() * e.dt();
    E = std::min(E, p->energy());
    E = std::min(E, c->storableEnergy());

    if (debugFeeding > 1)
      std::cerr << "Transfering " << E << " = min(" << p->energy() << ", "
                << c->storableEnergy() << ", " << dE << " * " << c->clockSpeed()
                << " * " << e.dt() << ") from " << p->id()
                << " to " << c->id() << " (" << p->energy()
                << " remaining)" << std::endl;

    c->feed(E);
    p->consumed(E);
  }

  void registerFeedEnd (Critter *c, Foodlet *p) {
    if (debugFeeding)
      std::cerr << "Feeding concluded by C" << c->id() << " on P" << p->id()
                << std::endl;

    e._feedingEvents.erase({c,p});
  }

  void watchForWarp (Critter *c, const Critter::FixtureData *fd) {
//    std::cerr << "Maybe teleport C" << c->id() << " (" << *fd << ") which is at "
//              << c->pos() << std::endl;
    if (fd->type == Critter::FixtureType::BODY) e._edgeCritters.insert(c);
  }

  void unwatchForWarp (Critter *c, const Critter::FixtureData *fd) {
//    std::cerr << "Maybe no longer teleport C" << c->id() << " (" << *fd
//              << ") which is at " << c->pos() << std::endl;
    if (fd->type == Critter::FixtureType::BODY) e._edgeCritters.erase(c);
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

  _physics.Step(float(dt()), V_ITER, P_ITER);

  std::set<std::pair<Critter*,uint>> destroyedSplines;
  for (const auto &f: _fightingEvents)
    processFight(f.first.first, f.first.second, f.second, destroyedSplines);
  for (const auto &p: destroyedSplines)
    p.first->destroySpline(p.second);

  for (Critter *c: _edgeCritters) maybeTeleport(c);

//  std::cerr << "Physical step took " << _physics.GetProfile().step
//            << " ms" << std::endl;
}

void Environment::createEdges(void) {
  static constexpr float W = .1, W2 = 2*W;
  real HS = extent();

  b2BodyDef edgesBodyDef;
  edgesBodyDef.type = b2_staticBody;
  edgesBodyDef.position.Set(0, 0);

  _edges = _physics.CreateBody(&edgesBodyDef);

  if (!boxEdges) { // Linear edges
    P2D edgesVertices [4] {
      { HS, HS }, { -HS, HS }, { -HS, -HS }, { HS, -HS }
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
    boxes[0].SetAsBox(W, HS+W2, {  HS+W,     0}, 0);
    boxes[1].SetAsBox(HS+W2, W, {     0,  HS+W}, 0);
    boxes[2].SetAsBox(W, HS+W2, { -HS-W,     0}, 0);
    boxes[3].SetAsBox(HS+W2, W, {     0, -HS-W}, 0);

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

void Environment::processFight(Critter *cA, Critter *cB, const FightingData &d,
                               std::set<std::pair<Critter*, uint>> &ds) {
  FightingDrawData fdd;
  fdd.vA = d.velocities[0];
  fdd.vB = d.velocities[1];

  // Ignore small enough collisions
  if (fdd.vA.Length() + fdd.vB.Length() < 1e-2) return;

  fdd.pA = cA->pos();
  fdd.pB = cB->pos();

  fdd.C = cB->pos() - cA->pos();
  fdd.C_ = fdd.C;
  fdd.C_.Normalize();

  /// TODO Improve !
  float n = d.fixtures.size();

  fdd.VA = b2Dot(fdd.vA,  fdd.C_) / n;
  fdd.VB = b2Dot(fdd.vB, -fdd.C_) / n;

  if (debugFighting > 1)
    std::cerr << "Fight step of " << cA->id() << " & " << cB->id() << "\n"
              << "\tat " << cA->pos() << " & " << cB->pos() << "\n"
              << "\t C = " << fdd.C << "\n"
              << "\tC' = " << fdd.C_ << "\n"
              << "\tVA = " << fdd.VA << " = " << fdd.vA << "."
                << fdd.C_ << "\n"
              << "\tVB = " << fdd.VB << " = " << fdd.vB << "."
                << fdd.C_ << "\n";
  if (debugFighting > 2)
    std::cerr << "Fixture-Fixture collisions details:\n";

  for (const auto &p: d.fixtures) {
    b2Fixture *fA = p.first, *fB = p.second;
    const Critter::FixtureData &fdA = *Critter::get(fA);
    const Critter::FixtureData &fdB = *Critter::get(fB);

    float alpha = (fB->GetDensity())
        / (fA->GetDensity() + fB->GetDensity());

    float N = config::Simulation::combatBaselineIntensity() * (
        cA->body().GetMass() * fdd.VA * fdd.VA
      + cB->body().GetMass() * fdd.VB * fdd.VB);

    float deA = alpha * .5 * N,
          deB = (1 - alpha) * .5 * N;

    if (debugFighting > 2)
      std::cerr << "\tFixtures C" << cA->id() << "F" << fdA << " and "
                << "C" << cB->id() << "F" << fdB << "\n"
                << "\t\trA = " << fA->GetDensity() << "\trB = "
                  << fB->GetDensity() << "\n"
                << "\t\t N = " << N << "\n"
                << "\t\tdA = " << deA << "\n"
                << "\t\tdB = " << deB << "\n";

    bool sADestroyed = cA->applyHealthDamage(fdA, deA, *this);
    if (sADestroyed)  ds.insert({cA, Critter::splineIndex(fdA)});

    bool sBDestroyed = cB->applyHealthDamage(fdB, deB, *this);
    if (sBDestroyed)  ds.insert({cB, Critter::splineIndex(fdB)});

  #ifndef NDEBUG
    fightingDrawData.push_back(fdd);
  #endif
  }
}

void Environment::maybeTeleport(Critter *c) {
  b2Body &b = c->body();
  auto a = b.GetAngle();
  P2D p0 = b.GetPosition(), p1 = p0;

//  std::cerr << "Critter " << c->id() << " touching edges at " << p0 << std::endl;

  if (p1.x < -extent())      p1.x += size();
  else if (p1.x > extent())  p1.x -= size();

  if (p1.y < -extent())      p1.y += size();
  else if (p1.y > extent())  p1.y -= size();

  if (p0 != p1) b.SetTransform(p1, a);

//  if (p0 != p1)
//    std::cerr << "Teleporting C" << c->id() << " from " << p0 << " to "
//              << p1 << std::endl;
}

decimal Environment::dt (void) {
  static const decimal DT = 1.f / config::Simulation::secondsPerDay();
  return DT;
}

} // end of namespace simu
