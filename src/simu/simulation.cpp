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

  _systemExpectedEnergy = data.ienergy;

  if (data.seed >= 0) _dice.reset(data.seed);

  _environment = std::make_unique<Environment>(egenome);
  _environment->modifyEnergyReserve(+data.ienergy);

  _gidManager.setNext(cgenome.id());
  cgenome.gdata.setAsPrimordial(_gidManager);

  float energy = data.ienergy;
  float cenergy = energy * data.cRatio;
  float energyPerCritter =
    std::min(Critter::energyForCreation(), cenergy / data.nCritters);

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
    addCritter(cg, x, y, a, energyPerCritter);
  }

  _nextPlantID = 0;

  plantRenewal(W * data.pRange);

  _statsLogger.open(config::Simulation::logFile());

  postInit();

  logStats();
}

Critter* Simulation::addCritter (const CGenome &genome,
                                 float x, float y, float a, float e) {

  b2BodyDef bodyDef;
  bodyDef.type = b2_dynamicBody;
  bodyDef.position.Set(x, y);
  bodyDef.angle = a;
  bodyDef.angularDamping = .95;
  bodyDef.linearDamping = .9;

  b2Body *body = physics().CreateBody(&bodyDef);

  float dissipatedEnergy =
    e * .5 *(1 - config::Simulation::healthToEnergyRatio());
  e -= dissipatedEnergy;
  _environment->modifyEnergyReserve(-e);

  Critter *c = new Critter (genome, body, e);

#ifndef NDEBUG
  float e_ = c->totalEnergy();
  if (e != e_)
    std::cerr << "Delta energy on C" << c->id() << " creation: " << e_-e
              << std::endl;
#endif

  _critters.insert(c);
  return c;
}

void Simulation::delCritter (Critter *critter) {
  _critters.erase(critter);
  physics().DestroyBody(&critter->body());
  delete critter;
}

Foodlet* Simulation::addFoodlet(BodyType t, float x, float y, float r, float e) {
  b2BodyDef bodyDef;
  bodyDef.type = b2_staticBody;
  bodyDef.position.Set(x, y);

  b2Body *body = _environment->physics().CreateBody(&bodyDef);
  Foodlet *f = new Foodlet (t, _nextPlantID++, body, r, e);

  _environment->modifyEnergyReserve(-e);

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

  correctFloatingErrors();
  if (_statsLogger) logStats();

  _time.next();

  steadyStateGA();
  if (_time.secondFraction() == 0)  plantRenewal();

  // TODO
//  if (_time.second() == 0)
//    (*_critters.begin())->body().ApplyForceToCenter(
//      4*config::Simulation::critterBaseSpeed()*fromPolar(M_PI/12,1), true);
}

void Simulation::produceCorpses(void) {
  std::vector<Critter*> corpses;
  for (Critter *c: _critters)
    if (c->isDead())
      corpses.push_back(c);

  for (Critter *c: corpses) {
    c->autopsy();
    Foodlet *f = addFoodlet(BodyType::CORPSE, c->x(), c->y(), c->bodyRadius(),
                            c->totalEnergy());
    f->setBaseColor(c->bodyColor());
    f->updateColor();
    delCritter(c);
  }
}

void Simulation::decomposition(void) {
  for (Foodlet *f: _foodlets) if (f->isCorpse())  f->update(*_environment);

  // Remove empty foodlets
  std::vector<Foodlet*> consumed;
  for (Foodlet *f: _foodlets)
    if (f->energy() <= 0)
      consumed.push_back(f); 

  for (Foodlet *f: consumed) delFoodlet(f);
}

void Simulation::plantRenewal(float bounds) {
  static const auto mR = config::Simulation::plantMinRadius(),
                    MR = config::Simulation::plantMaxRadius();
  static const auto minE = Foodlet::maxStorage(BodyType::PLANT, mR);

  // Maybe pop-up a new plant
  while (_environment->energy() > minE) {
    float r = _dice(mR, MR);
    float e = std::min(_environment->energy(),
                       Foodlet::maxStorage(BodyType::PLANT, r));
    float E = (bounds > 0 ? bounds : _environment->extent()) - r;
    addFoodlet(BodyType::PLANT, _dice(-E, E), _dice(-E, E), r, e);

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

  if (_startTime == _time)
    _statsLogger << "Step Date"
                    " NCritters NCorpses NPlants"
                    " NFeeding NFights"
                    " ECritters ECorpses EPlants EReserve\n";

  _statsLogger << _time.timestamp() << " " << _time.pretty() << " "

               << s.ncritters << " " << s.ncorpses << " " << s.nplants << " "
               << s.nfeedings << " " << s.nfights << " "

               << s.ecritters << " " << s.ecorpses << " " << s.eplants << " "
               << s.ereserve

               << std::endl;

  processStats(s);
}

float Simulation::totalEnergy(void) const {
  float e = 0;
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

      while (_environment->energy() >= minE) {
        auto genome = _ssga.getRandomGenome(_dice);
        float a = _dice(0., 2*M_PI);
        float x = _dice(-r, r), y = _dice(-r, r);
        addCritter(genome, a, x, y, minE);
      }
    }
  }
}

void Simulation::correctFloatingErrors(void) {
  float E = totalEnergy(), dE = E - _systemExpectedEnergy;
  if (dE > 1e-3)
    std::cerr << "Chaos generated " << std::showpos << dE << std::endl;
}

} // end of namespace simu
