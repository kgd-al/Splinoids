#include "kgd/random/dice.hpp"

#include "simulation.h"

namespace simu {

Simulation::Simulation(void) : _environment(nullptr) {}

Simulation::~Simulation (void) {
  clear();
}

void Simulation::init(const Environment::Genome &egenome,
                      Critter::Genome cgenome,
                      const InitData &data) {

  // TODO Remove (debug)
//  cgenome.vision.angleBody = M_PI/4.;
//  cgenome.vision.angleRelative = -M_PI/4;
//  cgenome.vision.width = M_PI/4.;
//  cgenome.vision.precision = 1;
  std::cerr << "Splinoid min energy: " << Critter::energyForCreation() << "\n"
            << "   Plant min energy: "
            << Foodlet::maxStorage(BodyType::PLANT,
                                   config::Simulation::plantMinRadius())
            << std::endl;

  _systemExpectedEnergy = data.ienergy;

  if (data.seed >= 0) _dice.reset(data.seed);

  _environment = std::make_unique<Environment>(egenome);
  _environment->modifyEnergyReserve(+data.ienergy);
  detectBudgetFluctuations(0);

  _gidManager.setNext(cgenome.id());
  cgenome.gdata.setAsPrimordial(_gidManager);

  decimal energy = data.ienergy;
  decimal cenergy = energy * data.cRatio;
  decimal energyPerCritter =
    std::min(Critter::energyForCreation(), decimal(cenergy / data.nCritters));

//  energy -= cenergy;
//  cenergy -= energyPerCritter * data.nCritters;
//  energy += cenergy;

  const float W = _environment->extent();
  const float C = W * data.cRange;

  for (uint i=0; i<data.nCritters; i++) {
    auto cg = cgenome.clone(_gidManager);
    cg.cdata.sex = (i%2 ? Critter::Sex::MALE : Critter::Sex::FEMALE);
    float a = _dice(0., 2*M_PI);
    float x = _dice(-C, C);
    float y = _dice(-C, C);

    // TODO
//    if (i == 0) x = 0, y = 0, a = 0;
//    if (i == 1) x = 2, y = 0, a = M_PI/2;

    if (i == 0) x = -49, y = 0, a = M_PI;

    addCritter(cg, x, y, a, energyPerCritter);
  }

  _nextPlantID = 0;

  plantRenewal(W * data.pRange);

  _statsLogger.open(config::Simulation::logFile());

  postInit();

  logStats();
}

Critter* Simulation::addCritter (const CGenome &genome,
                                 float x, float y, float a, decimal e) {

  b2BodyDef bodyDef;
  bodyDef.type = b2_dynamicBody;
  bodyDef.position.Set(x, y);
  bodyDef.angle = a;
  bodyDef.angularDamping = .95;
  bodyDef.linearDamping = .9;

  b2Body *body = physics().CreateBody(&bodyDef);

//  b2FrictionJointDef frictionDef;
//  frictionDef.Initialize(body, _environment->body(), {0,0});
//  frictionDef.maxForce = 1;
//  frictionDef.maxTorque = 1;
//  frictionDef.

  // Seems buggy...
//  _environment->physics().CreateJoint(&frictionDef);

#ifndef NDEBUG
  decimal prevEE = _environment->energy();
#endif

  decimal dissipatedEnergy =
    e * .5 *(1 - config::Simulation::healthToEnergyRatio());
  decimal e_ = e - dissipatedEnergy;
  _environment->modifyEnergyReserve(-e_);

  Critter *c = new Critter (genome, body, e_);
  _critters.insert(c);

#ifndef NDEBUG
  decimal e__ = c->totalEnergy();
  if (e_ != e__)
    std::cerr << "Delta energy on C" << c->id() << " creation: " << e__-e_
              << std::endl;
  decimal currEE = _environment->energy(), dEE = currEE - prevEE;
  if (dEE != e_)
    std::cerr << "Delta energy reserve on C" << c->id() << " creation: "
              << dEE +e_ << std::endl;
#endif

  return c;
}

void Simulation::delCritter (Critter *critter) {
  _critters.erase(critter);
  physics().DestroyBody(&critter->body());
  delete critter;
}

Foodlet* Simulation::addFoodlet(BodyType t, float x, float y, float r, decimal e) {
  b2BodyDef bodyDef;
  bodyDef.type = b2_staticBody;
  bodyDef.position.Set(x, y);

  b2Body *body = _environment->physics().CreateBody(&bodyDef);
  Foodlet *f = new Foodlet (t, _nextPlantID++, body, r, e);

  _foodlets.insert(f);
  return f;
}

void Simulation::delFoodlet(Foodlet *foodlet) {
  _foodlets.erase(foodlet);
  physics().DestroyBody(&foodlet->body());
  delete foodlet;
}

void Simulation::clear (void) {
  while (!_critters.empty())  delCritter(*_critters.begin());
  while (!_foodlets.empty())  delFoodlet(*_foodlets.begin());
}

void Simulation::step (void) {
//  std::cerr << "\n## Simulation step " << _time.timestamp() << " ("
//            << _time.pretty() << ") ##" << std::endl;

  _minGen = std::numeric_limits<uint>::max();
  _maxGen = 0;

  for (Critter *c: _critters) {
    c->step(*_environment);

    _minGen = std::min(_minGen, c->genotype().gdata.generation);
    _maxGen = std::max(_maxGen, c->genotype().gdata.generation);
  }

  if (_ssga.watching()) _ssga.prePhysicsStep(_critters);

  _environment->step();

  if (_ssga.watching()) _ssga.postPhysicsStep(_critters);

  produceCorpses();
  decomposition();

  if (_statsLogger) logStats();

  _time.next();

  steadyStateGA();
  if (_time.secondFraction() == 0)  plantRenewal();

  if (false) {
  /* TODO DEBUG AREA
  */
    Critter *c1 = nullptr;
    for (Critter *c_: _critters) {
      if (c_->id() == Critter::ID(1)) {
        c1 = c_;
        break;
      }
    }
//    assert(c1);
    if (c1) {
//      if (_time.timestamp() == 4) {
//        c1->applyHealthDamage(
//            Critter::FixtureData(Critter::FixtureType::BODY, {0,0,0}),
//            .025,  *_environment);
//      }
      if (_time.day() == 0)
        c1->body().ApplyForceToCenter(
          4*config::Simulation::critterBaseSpeed()*fromPolar(0*M_PI/12,1), true);
    }
  /*
  */
  }

  // Fails when system energy diverges too much from initial value
  detectBudgetFluctuations();

  // If above did not fail, effect small corrections to keep things smooth
  if (config::Simulation::screwTheEntropy())  correctFloatingErrors();
}

void Simulation::produceCorpses(void) {
  std::vector<Critter*> corpses;
  for (Critter *c: _critters)
    if (c->isDead())
      corpses.push_back(c);

  detectBudgetFluctuations();

  for (Critter *c: corpses) {
    c->autopsy();
    Foodlet *f = addFoodlet(BodyType::CORPSE, c->x(), c->y(), c->bodyRadius(),
                            c->totalEnergy());
    f->setBaseColor(c->bodyColor());
    f->updateColor();
    delCritter(c);
  }

  detectBudgetFluctuations();
}

void Simulation::decomposition(void) {
  for (Foodlet *f: _foodlets) if (f->isCorpse())  f->update(*_environment);

  // Remove empty foodlets
  std::vector<Foodlet*> consumed;
  for (Foodlet *f: _foodlets)
    if (f->energy() <= 0)
      consumed.push_back(f), assert(f->energy() >= 0);

  for (Foodlet *f: consumed) delFoodlet(f);
}

void Simulation::plantRenewal(float bounds) {
  static const auto mR = config::Simulation::plantMinRadius(),
                    MR = config::Simulation::plantMaxRadius();
  static const auto minE = Foodlet::maxStorage(BodyType::PLANT, mR);

  // Maybe pop-up a new plant
  while (_environment->energy() > minE) {
    float r = _dice(mR, MR);
    decimal e = std::min(_environment->energy(),
                         Foodlet::maxStorage(BodyType::PLANT, r));
    float E = (bounds > 0 ? bounds : _environment->extent()) - r;
    addFoodlet(BodyType::PLANT, _dice(-E, E), _dice(-E, E), r, e);    
    _environment->modifyEnergyReserve(-e);

    assert(_environment->energy() >= 0);
  }
}

void Simulation::logStats (void) {
  Stats s;
  s.nplants = 0;
  s.ncorpses = 0;
  s.eplants = 0;
  s.ecorpses = 0;

  for (const auto &f: _foodlets) {
    if (f->type() == simu::BodyType::PLANT)
          s.nplants++,  s.eplants += f->energy();
    else  s.ncorpses++, s.ecorpses += f->energy();
  }

  s.ncritters = _critters.size();

  s.nfeedings = _environment->feedingEvents().size();
  s.nfights = _environment->fightingEvents().size();

  s.ecritters = 0;
  for (const auto &c: _critters) s.ecritters += c->totalEnergy();

  s.ereserve = _environment->energy();

  decimal eE = totalEnergy() - _systemExpectedEnergy;

  if (_startTime == _time)
    _statsLogger << "Step Date"
                    " NCritters NCorpses NPlants"
                    " NFeeding NFights"
                    " ECritters ECorpses EPlants EReserve"
                    " eE"
                    "\n";

  _statsLogger << _time.timestamp() << " " << _time.pretty() << " "

               << s.ncritters << " " << s.ncorpses << " " << s.nplants << " "
               << s.nfeedings << " " << s.nfights << " "

               << s.ecritters << " " << s.ecorpses << " " << s.eplants << " "
               << s.ereserve << " "
               << eE

               << std::endl;

  processStats(s);
}

decimal Simulation::totalEnergy(void) const {
  decimal e = 0;
  for (const auto &c: _critters)  e += c->totalEnergy();
  for (const auto &f: _foodlets)  e += f->energy();
  e += _environment->energy();
  return e;
}

void Simulation::steadyStateGA(void) {
  static const auto ssgaMPS = config::Simulation::ssgaMinPopSize();
  bool wasWatching = _ssga.watching();
  _ssga.setWatching(_critters.size() < 2*ssgaMPS);

  if (wasWatching && !_ssga.watching())
    _ssga.clear();

  else {
    static const auto minE = Critter::energyForCreation();
    _ssga.setActive(_critters.size() < ssgaMPS);
    if (_ssga.active() && _time.secondFraction() == 0) {
      float r = _environment->extent() - Critter::MIN_SIZE * Critter::RADIUS;

      while (_environment->energy() >= minE && _critters.size() < ssgaMPS) {
        auto genome = _ssga.getRandomGenome(_dice, 5000);
        genome.gdata.setAsPrimordial(_gidManager);
        float a = _dice(0., 2*M_PI);
        float x = _dice(-r, r), y = _dice(-r, r);
        addCritter(genome, x, y, a, minE);
      }
    }
  }
}

void Simulation::correctFloatingErrors(void) {
  static constexpr auto epsilonE = config::Simulation::epsilonE;
  static constexpr decltype(epsilonE) epsilonE_ = epsilonE / 10.f;
  decimal E = totalEnergy(), dE = E - _systemExpectedEnergy;
  if (epsilonE_ <= dE && dE <= _environment->energy()) {
    _environment->modifyEnergyReserve(-dE);
    std::cerr << "Corrected ambiant chaos by injecting " << (dE > 0 ? "+" : "")
              << dE << " in the environment" << std::endl;
  }
}

void Simulation::detectBudgetFluctuations(float threshold) {
  decimal E = totalEnergy(), dE = _systemExpectedEnergy-E;
  if (std::fabs(dE) > threshold) {
    std::cerr << std::setprecision(std::numeric_limits<decimal>::digits10)
              << "Excessive budget variation of "
              << E-_systemExpectedEnergy << " = " << E << " - "
              << _systemExpectedEnergy << std::endl;
    assert(false);
  }
}

} // end of namespace simu
