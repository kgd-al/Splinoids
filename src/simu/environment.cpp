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

  using FeedingEvent = Environment::FeedingEvent;
  Environment::FeedingEvents &fe;

  using FightingKey = Environment::FightingKey;
  using FightingData = Environment::FightingData;
  Environment::FightingEvents &fi;

  Environment::PendingDeletions &pd;

  using FightingDrawData = Environment::FightingDrawData;

  IFDEBUG(std::vector<FightingDrawData> &fddVector);

  float dt;

  CollisionMonitor (Environment::FeedingEvents &fe,
                    Environment::FightingEvents &fi,
                    Environment::PendingDeletions &pd,
                    IFDEBUGCOMMA(std::vector<FightingDrawData> &fdd)
                    float dt)
    : fe(fe), fi(fi), pd(pd), IFDEBUGCOMMA(fddVector(fdd)) dt(dt)  {}

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
    Critter *cA = critter(dA), *cB = critter(dB);
    Foodlet *pA = foodlet(dA), *pB = foodlet(dB);

    if (cA && cB)       registerFightStart(cA, fA, cB, fB);
    else if (cA && pB)  registerFeedStart(cA, fA, pB);
    else if (cB && pA)  registerFeedStart(cB, fB, pA);
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

    fi[{cA,cB}].insert({fA,fB});
  }

  void processFightStep (Critter *cA, b2Fixture *fA,
                         Critter *cB, b2Fixture *fB) {

    Environment::FightingDrawData fdd;
    fdd.vA = cA->body().GetLinearVelocity();
    fdd.vB = cB->body().GetLinearVelocity();

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

    bool aSplineDestroyed = cA->applyHealthDamage(fdA, deA);
    if (aSplineDestroyed) pd.insert({cA, fA});

    bool bSplineDestroyed = cB->applyHealthDamage(fdB, deB);
    if (bSplineDestroyed) pd.insert({cB, fB});

#ifndef NDEBUG
    fddVector.push_back(fdd);
#endif
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
    auto it = fi.find(k);
    if (it != fi.end()) {
      it->second.erase({fA,fB});
      if (it->second.empty())
        fi.erase(it);
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

    fe.insert({c,p});
  }

  void processFeedStep (Critter *c, b2Fixture *f, Foodlet *p) {
    static const float &dE = config::Simulation::energyAbsorptionRate();
    if (f->GetShape()->GetType() != b2Shape::e_circle)  return;
    if (p->energy() <= 0) return;

    float e = std::min(dE * dt, p->energy());
    e = std::min(e, c->storableEnergy());
    c->feed(e);
    p->consumed(e);

    if (debugFeeding > 1)
      std::cerr << "Transfering " << e << " from " << p->id()
                << " to " << c->id() << " (" << p->energy()
                << " remaining)" << std::endl;
  }

  void registerFeedEnd (Critter *c, Foodlet *p) {
    if (debugFeeding)
      std::cerr << "Feeding concluded by C" << c->id() << " on P" << p->id()
                << std::endl;

    fe.erase({c,p});
  }
};

Environment::Environment(const Genome &g)
  : _genome(g), _physics({0,0}),
    _cmonitor(new CollisionMonitor(_feedingEvents, _fightingEvents,
                                   _pendingDeletions,
                                   IFDEBUGCOMMA(fightingDrawData) dt())) {

  createEdges();
  _physics.SetContactListener(_cmonitor);

  _energyReserve = 0;
}

Environment::~Environment (void) {
  _physics.DestroyBody(_edges);
  delete _cmonitor;
}

void Environment::step (void) {

  // Box2D parameters
  static const int V_ITER = config::Simulation::b2VelocityIter();
  static const int P_ITER = config::Simulation::b2PositionIter();

#ifndef NDEBUG
  fightingDrawData.clear();
#endif
  _pendingDeletions.clear();

  _physics.Step(dt(), V_ITER, P_ITER);

  for (const auto &d: _pendingDeletions)
    d.first->destroySpline(d.second);

//  std::cerr << "Physical step took " << _physics.GetProfile().step
//            << " ms" << std::endl;
}

void Environment::createEdges(void) {
  real HS = extent();
  P2D edgesVertices [4] {
    { HS, HS }, { -HS, HS }, { -HS, -HS }, { HS, -HS }
  };

  b2ChainShape edgesShape;
  edgesShape.CreateLoop(edgesVertices, 4);

  b2BodyDef edgesBodyDef;
  edgesBodyDef.type = b2_staticBody;
  edgesBodyDef.position.Set(0, 0);

  _edges = _physics.CreateBody(&edgesBodyDef);
  _edges->CreateFixture(&edgesShape, 0);

  _edgesUserData.type = BodyType::OBSTACLE;
  _edgesUserData.ptr.obstacle = nullptr;
  _edges->SetUserData(&_edgesUserData);
}

float Environment::dt (void) {
  static const float DT = 1.f / config::Simulation::secondsPerDay();
  return DT;
}

} // end of namespace simu
