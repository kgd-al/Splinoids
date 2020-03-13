#include "kgd/random/dice.hpp"

#include "simulation.h"

namespace simu {

Simulation::Simulation(void) : _environment(nullptr) {}

Simulation::~Simulation (void) {
  clear();
}


struct CritterInitData {
  Critter::Genome genome;
  float x, y;
  float size;
};
using InitList = std::vector<CritterInitData>;
using InitMethod = std::function<void(
  const Critter::Genome&, phylogeny::GIDManager&, InitList&)>;

static const std::map<InitType,InitMethod> initMethods = {
  // == Test multiple mutants
  { InitType::MUTATIONAL_LANDSCAPE,
    [] (const Critter::Genome &base, phylogeny::GIDManager &gidm, InitList &l) {
      int Nx = 1, Ny = 1;
      rng::FastDice dice;// (0);
      for (int i=-Nx; i<=Nx; i++) {
        for (int j=-Ny; j<=Ny; j++) {
          auto cg = base.clone(gidm);
          if (i != 0 || j != 0)
            for (int k=0; k<100; k++) cg.mutate(dice);
          cg.cdata.sex = Critter::Sex::FEMALE;
          l.push_back({cg, 4.5f*i-1, 2.5f*j, 1});

          cg = cg.clone(gidm);
          cg.cdata.sex = Critter::Sex::MALE;
          l.push_back({cg, 4.5f*i+1, 2.5f*j, 1});
        }
      }
    }
  },

  // == "Random" init
  { InitType::RANDOM,
    [] (const Critter::Genome&, phylogeny::GIDManager &gidm, InitList &l) {
      int Nx = 2, Ny = 1;
      rng::FastDice dice;// (0);
      for (int i=-Nx; i<=Nx; i++) {
        for (int j=-Ny; j<=Ny; j++) {
          auto cg = Critter::Genome::random(dice);

//          cg.splines[0] = genotype::Spline({
//            M_PI/2,
//            -M_PI/4, .5,
//            .25, .25, .75, -.1,
//            .1, .5, .5
//          });

          cg.gdata.updateAfterCloning(gidm);

          for (int m=0; m<1000; m++)  cg.mutate(dice);

          l.push_back({cg, 2.5f*i, 2.5f*j, 1});
        }
      }
    }
  },

  // == "Random" init
  { InitType::MEGA_RANDOM,
    [] (const Critter::Genome&, phylogeny::GIDManager &gidm, InitList &l) {
      using CGenome = Critter::Genome;
      using Sex = genotype::BOCData::Sex;
      using Spline = genotype::Spline;

      int Nx = 2, Ny = 1;
      rng::FastDice dice (4);
      for (int i=-Nx; i<=Nx; i++) {
        for (int j=-Ny; j<=Ny; j++) {
          CGenome cg;
          cg.gdata.updateAfterCloning(gidm);
          cg.cdata = genotype::BOCData::random(dice);

          const auto &sb = config::EDNAConfigFile<Spline>::dataBounds();
          for (uint i=0; i<CGenome::SPLINES_COUNT; i++) {
            for (uint j=0; j<std::tuple_size_v<Spline::Data>; j++)
              cg.splines[i].data[j] = dice(sb.min[j], sb.max[j]);
          }

          for (uint i=0; i<2*CGenome::SPLINES_COUNT; i++)
            cg.dimorphism[i] = dice(0, 1);

          cg.cdata.sex = config::MutationSettings::BoundsOperators<genotype::BOCData::Sex>::rand(Sex(0),Sex(0),dice);

          // DEBUG !!
//          for (uint i=0; i<2*CGenome::SPLINES_COUNT; i++)
//            cg.dimorphism[i] = 0;
//          for (uint i=1; i<2; i++)  cg.dimorphism[i] = 1;
//          cg.cdata.sex = Sex::FEMALE;
          // ====

          if (!cg.check())
            utils::doThrow<std::logic_error>(
              "Invalid random point in ", utils::className<CGenome>(),
              "'s genetic space: ", cg);

//          std::cerr << "Random genome " << cg.id() << " at " << 2.5f*i << ","
//                    << 2.5f*j << std::endl;

          l.push_back({cg, 2.5f*i, 2.5f*j, 1});
        }
      }
    }
  },

  // == "Regular" init
  { InitType::REGULAR,
    [] (const Critter::Genome &base, phylogeny::GIDManager &gidm, InitList &l) {
      rng::FastDice dice (0);
      for (int i=0; i<10; i++) {
        auto cg = base.clone(gidm);
        cg.cdata.sex = (i%2 ? Critter::Sex::MALE : Critter::Sex::FEMALE);
        l.push_back({cg, dice(-10.f,10.f), dice(-10.f,10.f), 1});
      }
    }
  }
};

void Simulation::init(const Environment::Genome &egenome,
                      Critter::Genome cgenome,
                      InitType type) {

  _environment = std::make_unique<Environment>(egenome);

  if (type == InitType::REGULAR) {
    _gidManager.setNext(cgenome.id());
    cgenome.gdata.setAsPrimordial(_gidManager);
  }

  auto it = initMethods.find(type);
  if (it == initMethods.end())
    utils::doThrow<std::invalid_argument>("Init type ", int(type),
                                          " is not a valid value");

  InitList ilist;
  it->second(cgenome, _gidManager, ilist);
  for (const auto &i: ilist)
    addCritter(i.genome, i.x, i.y, i.size);

  postInit();
}

Critter* Simulation::addCritter (const CGenome &genome,
                                 float x, float y, float r) {

  Critter *c = new Critter (genome, x, y, r);
  _critters.insert(c);
  return c;
}

void Simulation::delCritter (Critter *critter) {
  _critters.erase(critter);
  delete critter;
}

void Simulation::clear (void) {
  while (!_critters.empty())  delCritter(*_critters.begin());
}

void Simulation::step (void) {

}

} // end of namespace simu
