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
  cgenome.vision.angleBody = M_PI/4.;
  cgenome.vision.angleRelative = -M_PI/4;
  cgenome.vision.width = M_PI/4.;
  cgenome.vision.precision = 10;

  if (data.seed >= 0) _dice.reset(data.seed);

  _environment = std::make_unique<Environment>(egenome);

  _gidManager.setNext(cgenome.id());
  cgenome.gdata.setAsPrimordial(_gidManager);

  float energy = data.ienergy;
  float cenergy = energy * data.cRatio;
  float energyPerCritter = std::min(Critter::storage(Critter::MIN_SIZE),
                                    cenergy / data.nCritters);

  energy -= cenergy;
  cenergy -= energyPerCritter * data.nCritters;
  energy += cenergy;

  const float C = data.cRange, P = data.pRange;

  for (uint i=0; i<data.nCritters; i++) {
    auto cg = cgenome.clone(_gidManager);
    cg.cdata.sex = (i%2 ? Critter::Sex::MALE : Critter::Sex::FEMALE);
    addCritter(cg, /*_dice(-C, C), _dice(-C, C)*/0,0, energyPerCritter);
  }

  while (energy > 0) {
    float s = _dice(1.f, 2.f);
    float e = std::min(Plant::maxStorage(s), energy);
    energy -= e;
    addPlant(_dice(-P, P), _dice(-P, P), s, e);
  }

  postInit();
}

Critter* Simulation::addCritter (const CGenome &genome,
                                 float x, float y, float e) {

  b2BodyDef bodyDef;
  bodyDef.type = b2_dynamicBody;
  bodyDef.position.Set(x, y);
  bodyDef.angle = _dice(0., 2*M_PI);
  bodyDef.angularDamping = .8;
  bodyDef.linearDamping = .5;

  b2Body *body = physics().CreateBody(&bodyDef);
  Critter *c = new Critter (genome, body, e);

  _critters.insert(c);
  return c;
}

void Simulation::delCritter (Critter *critter) {
  _critters.erase(critter);
  physics().DestroyBody(&critter->body());
  delete critter;
}

Plant* Simulation::addPlant(float x, float y, float s, float e) {
  b2BodyDef bodyDef;
  bodyDef.type = b2_staticBody;
  bodyDef.position.Set(x, y);

  b2Body *body = _environment->physics().CreateBody(&bodyDef);
  Plant *p = new Plant (body, s, e);

  _plants.insert(p);
  return p;
}

void Simulation::delPlant(Plant *plant) {
  _plants.erase(plant);
  physics().DestroyBody(&plant->body());
  delete plant;
}

void Simulation::clear (void) {
  while (!_critters.empty())  delCritter(*_critters.begin());
  while (!_plants.empty())  delPlant(*_plants.begin());
}

void Simulation::step (void) {
  _minGen = std::numeric_limits<uint>::max();
  _maxGen = 0;

  for (Critter *c: _critters) {
    c->step(*_environment);

    _minGen = std::min(_minGen, c->genotype().gdata.generation);
    _maxGen = std::max(_maxGen, c->genotype().gdata.generation);
  }

  _environment->step();
  _time.next();
//  std::cerr << "Simulation step " << _time.pretty() << std::endl;
}

} // end of namespace simu
