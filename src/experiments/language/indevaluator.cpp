#include "indevaluator.h"

namespace simu {

std::atomic<bool> Evaluator::aborted = false;

Evaluator::Evaluator (Scenario::Type t) {
  params.type = t;

  using T = Scenario::Type;
  if (t == T::DIRECTION) {
    params.specs = {{0}, {1}, {2}};
  } else
    utils::Thrower("Unhandled spec type '", EnumUtils<T>::getName(t), "'");
}

std::string Evaluator::prettyEvalTypes(void) {
  using T = Scenario::Type;
  using U = EnumUtils<T>;
  std::ostringstream oss;
  oss << "[ ";
  for (auto t: U::iteratorUValues()) if (t) oss << U::getName(t) << " ";
  oss << "]";
  return oss.str();
}

void Evaluator::applyNeuralFlags(phenotype::ANN &ann,
                                 const std::string &tagsfile) {
//  auto &n = ann.neurons();

//  std::ifstream ifs (tagsfile);
//  if (!ifs)
//    utils::Thrower("Failed to open neural tags file '", tagsfile, "'");

//  for (std::string line; std::getline(ifs, line); ) {
//    if (line.empty() || line[0] == '/') continue;
//    std::istringstream iss (line);
//    phenotype::Point pos;
//    iss >> pos;

//    auto it = n.find(pos);
//    if (it == n.end())
//      utils::Thrower("No neuron at position {", pos.x(), ", ", pos.y(), "}");

//    iss >> (*it)->flags;
//  }

//  // Also aggregate similar inputs/outputs
//  for (auto &p: ann.neurons()) {
//    using P = phenotype::Point;
//    phenotype::ANN::Neuron &n = *p;
//    P pos = n.pos;
//    auto &f = n.flags;

//    if (n.type == phenotype::ANN::Neuron::I) {
//      if (pos == P{0.f, -1.f, -.5f} || pos == P{0.f, -1.f, -1.f})
//        f = 1<<18;  // health
//      else if (pos.z() <= -1/3.) {
//        f = 1<<19;  // audition
//        if (pos.x() > 0)  f |= 1<<17;  // right
//      } else if (pos.z() >= 1/3.) {
//        f = 1<<20;  // vision
//        if (pos.x() > 0)  f |= 1<<17;  // right
//      } else {
//        f = 1<<21;  // touch
//        if (pos.x() > 0)  f |= 1<<17;  // right
//      }
//      if (f) f |= 1<<16; // input

//    } else if (n.type == phenotype::ANN::Neuron::O) {
//      if (pos.z() >= .5)
//        f = 1 << 26;  // Voice
//      else if (pos.z() == -.5f) {
//        f = 1<<27;    // arms
//        if (pos.x() > 0) f |= 1<<25;  // right
//      }
//      if (f) f |= 1<<24; // output
//    }
//  }

//  if (config::Simulation::verbosity() > 0) {
//    std::cout << "Neural flags:\n";
//    for (const auto &p: n)
//      std::cout << "\t" << std::setfill(' ') << std::setw(10) << p->pos
//                << ":\t"
//                << std::bitset<8*sizeof(p->flags)>(p->flags)
//                << "\n";
//  }
}

auto duration (const simu::Simulation &s) {
  static const auto &DT = config::Simulation::ticksPerSecond();
  return float(s.currTime().timestamp()) / DT;
}

template <typename T, typename ...ARGS>
auto avg (const std::set<Critter*> &pop,
          T (Critter::*getter) (ARGS...) const,
          ARGS ...args) {
  T v = 0;
  if (pop.size() > 0) {
    for (Critter *c: pop)  v += (c->*getter)(std::forward<ARGS>(args)...);
    v /= T(pop.size());
  }
  return v;
}

// === Logging
/// TODO Log (what to log?)

struct Evaluator::LogData {
  std::ofstream traj, adata, // fight / accoustics
                ndata; // hidden neurons / modules

  std::vector<std::ofstream> idata, odata, // neural i/o
                             mdata; // modules
};

Evaluator::LogData* Evaluator::logging_getData(void) {
  return new Evaluator::LogData;
}

void Evaluator::logging_freeData(LogData **d) {
  delete *d;
  *d = nullptr;
}

void Evaluator::logging_init(LogData *d, const stdfs::path &f,
                             Scenario &s) {
  stdfs::create_directories(f);
  std::cerr << f << " should exist!\n";

  static const auto NEURAL_FLAGS = "OFNCBATHI";

  const auto filename = [] (const std::string &name,
                            const std::string &ext,
                            uint i) {
    std::string id = utils::mergeToString("_", Scenario::critterRole(i));
    return utils::mergeToString(name, id, ".", ext);
  };

  const auto &critters = s.critters();

  d->traj.open(f / "trajectory.dat");
  d->traj << "t X Y A\n";

  d->adata.open(f / "acoustics.dat");
  auto &alog = d->adata;
  for (uint i=0; i<critters.size(); i++) {
    std::string prefix = std::string(1, std::toupper(Scenario::critterRole(i)));
    for (auto s: {'L', 'R'}) {
      alog << prefix << "I" << s << "N ";
      for (uint i=0; i<Critter::VOCAL_CHANNELS; i++)
        alog << prefix << "I" << s << i << " ";
    }
    alog << prefix << "ON ";
    for (uint i=0; i<Critter::VOCAL_CHANNELS; i++)
      alog << prefix << "O" << i << " ";
  }
  alog << "\n";

  d->idata.resize(critters.size());
  d->odata.resize(critters.size());
  for (uint i=0; i<critters.size(); i++) {
    auto &ilog = d->idata[i];
    ilog.open(f / filename("inputs", "dat", i));
    for (auto s: critters[0]->neuralInputsHeader()) ilog << s << " ";
    ilog << "\n";

    auto &olog = d->odata[i];
    olog.open(f / filename("outputs", "dat", i));
    for (auto s: critters[0]->neuralOutputsHeader()) olog << s << " ";
    olog << "\n";
  }

  if (s.neuralEvaluation() && annTagsFile.empty()) {
    auto &nlog = d->ndata;
    nlog.open(f / "neurons.dat");
    nlog << NEURAL_FLAGS;
    for (const auto &p: critters[0]->brain().neurons())
      if (p->isHidden())
        nlog << " (" << p->pos << ")";
    nlog << "\n";
  }

  if (!annTagsFile.empty()) {
    auto &mlogs = d->mdata;
    mlogs.resize(critters.size());
    for (uint i=0; i<mlogs.size(); i++) {
      auto &mlog = mlogs[i];
      mlog.open(f / filename("modules", "dat", i));
      mlog << NEURAL_FLAGS;
      for (const auto &p: manns[i]->modules()) {
        if (p.second->type() == phenotype::ANN::Neuron::H) {
          auto f = p.second->flags;
          mlog << " " << f << "M " << f << "S";
        }
      }
      mlog << "\n";
    }
  }

  {
    std::ofstream blog (f / "brain.dat");
    const phenotype::ANN &b = critters[0]->brain();
    blog << "Type X Y Z Depth Flags\n";
    for (const phenotype::ANN::Neuron::ptr &p: b.neurons()) {
      blog << p->type
           << " " << p->pos.x() << " " << p->pos.y() << " " << p->pos.z()
           << " " << p->depth << " " << p->flags << "\n";
    }

#ifdef WITH_GVC
//    b.render_gvc_graph(f / "brain.dot");
#endif
  }
}

void Evaluator::logging_step(LogData *d, Scenario &s) {
  const auto &critters = s.critters();

  if (d->adata.is_open()) {
    auto &alog = d->adata;
    for (const simu::Critter *c: critters) {
      for (auto v: c->ears()) alog << v << " ";
      for (auto v: c->producedSound()) alog << v << " ";
    }
    alog << "\n";
  }

  for (uint i=0; i<critters.size(); i++) {
    auto &ilog = d->idata[i];
    if (ilog.is_open()) {
      for (const auto &v: critters[i]->neuralInputs()) ilog << v << " ";
      ilog << "\n";
    }

    auto &olog = d->odata[i];
    if (olog.is_open()) {
      for (const auto &v: critters[i]->neuralOutputs()) olog << v << " ";
      olog << "\n";
    }
  }

  if (s.neuralEvaluation()) {
    auto &os = d->ndata;
    os << s.currentFlags();
    for (const auto &p: critters[0]->brain().neurons())
      if (p->isHidden())
        os << " " << p->value;
    os << "\n";
  }

  for (uint i=0; i<d->mdata.size(); i++) {
    auto &os = d->mdata[i];
    os << s.currentFlags();
    for (const auto &p: manns[i]->modules()) {
      if (p.second->type() == phenotype::ANN::Neuron::H) {
        const auto &v = p.second->value();
        os << " " << v.mean << " " << v.stddev;
      }
    }
    os << "\n";
  }
}

// ===

std::string Evaluator::Params::toString(const Scenario::Params &p) {
  using Type = Scenario::Type;
  std::ostringstream oss;
  switch (p.type) {
  case Type::DIRECTION:
    oss << "d_" << std::array<char,3>{{'l','f','r'}}[p.spec.target];
    break;
  case Type::COLOR:     oss << "c"; break;
  default:
    utils::Thrower("Unable to generate string representation");
  }

  return oss.str();
}

Evaluator::Params Evaluator::Params::fromArgv (const std::string &scenario) {
  Evaluator::Params params;

  using Type = Scenario::Type;
  params.type = Type::ERROR;
  switch (scenario[0]) {
  case 'd': params.type = Type::DIRECTION; break;
  case 'c': params.type = Type::COLOR; break;
  default:
    utils::Thrower("Unknown scenario type ", scenario[0]);
  }

  if (scenario.size() == 1) {
    if (params.type == Type::DIRECTION)
      params.specs = {{0},{1},{2}};
    else
      utils::Thrower("Unhandled scenario");

  } else if (scenario.size() == 3) {
    Scenario::Spec spec {};
    if (params.type == Type::DIRECTION) {
      switch (scenario[2]) {
      case 'l': spec.target = 0; break;
      case 'f': spec.target = 1; break;
      case 'r': spec.target = 2; break;
      default:
        utils::Thrower("Unknown scenario direction subtype ", scenario[2]);
      }
    } else if (params.type == Type::COLOR) {
      std::cerr << "Color spec is brick-implemented\n";
      spec.target = 1;
  //    spec.colors = {{ {1,0,0}, {0,1,0}, {0,0,1} }};
    } else
      utils::Thrower("Unrecognized scenario argument '", scenario, "'");
    params.specs = {spec};

  } else
    utils::Thrower("Malformed scenario specification '", scenario, "'");


//  if (neuralEvaluation(scenario)) {
//    params.flags.reset();

//    using F = simu::Scenario::Params::Flag;
//    std::string s = scenario;
//    if (s.size() != 4) utils::Thrower("Scenario '", s, "' has unexpected size");

//    bool ok = true;
//    switch (s[1]) {
//    case '1':
//      switch (s[3]) {
//      case 'a': params.flags.set(F::PAIN_ABSL); break;
//      case 'i': params.flags.set(F::PAIN_INST); break;
//      case 't': params.flags.set(F::TOUCH); break;
//      default: ok = false;
//      }
//      break;

//    case '2':
//      switch (s[3]) {
//      case 'a': params.flags.set(F::WITH_ALLY); break;
//      case 'b': params.flags.set(F::WITH_OPP1); break;
//      case 'c': params.flags.set(F::WITH_OPP2); break;
//      default: ok = false;
//      }
//      break;

//    case '3':
//      switch (s[3]) {
//      case 'n': params.flags.set(F::SOUND_NOIS); break;
//      case 'f': params.flags.set(F::SOUND_FRND); break;
//      case 'o': params.flags.set(F::SOUND_OPPN); break;
//      default: ok = false;
//      }
//      if (scenarioArg.empty() && s[3] != 'n')
//        utils::Thrower("Type 3 scenario requires an additional argument");
//      else
//        params.scenarioArg = scenarioArg;
//      break;

//    default: ok = false;
//    }

//    if (!params.flags.any() || !ok)
//      utils::Thrower("Invalid scenario '", scenario, "'");
//    assert(params.flags.any() && ok);
//  }

  return params;
}

// ===

bool Evaluator::neuralEvaluation(const std::string &scenario) {
  return !scenario.empty() && scenario.substr(0, 2) != "ne";
}

void Evaluator::operator () (Ind &ind) {
  operator() (ind, params);
}

Evaluator::Footprint Evaluator::footprint(const Params &p) {
  size_t s =
      1 // axons cost
      + p.specs.size() * (
          4 // neurons (emitter/receiver), voice, motors energy consumption
        + config::Simulation::ticksPerSecond()
          // Store 1st second of emitter's talk
      );
  return Footprint(s, NAN);
}

std::vector<std::string> Evaluator::footprintFields (const Params &p) {
  std::vector<std::string> v (footprint(p).size(), "UNASSIGNED");
  uint f = 0;

  v[f++] = "e_ax";

  for (uint i=0; i<p.specs.size(); i++) {
    auto l = Params::toString(p.scenarioParams(i));
    v[f++] = "e_an_e_" + l;
    v[f++] = "e_an_r_" + l;
    v[f++] = "e_vc_e" + l;
    v[f++] = "e_mt_r" + l;

    for (uint j=0; j<config::Simulation::ticksPerSecond(); j++)
      v[f++] = utils::mergeToString("v_", l, "_s", j);
  }

  return v;
}

Scenario::Params Evaluator::Params::scenarioParams (uint i) const {
  Scenario::Params p;
  p.type = type;
  p.spec = specs[i];
  p.flags.reset();
  return p;
}

void Evaluator::operator() (Ind &ind, Params &params) {
  bool brainless, mute = false;

//  using utils::operator<<;
//  for (int lesion: lesions) {

  uint n = params.specs.size();
  std::vector<float> scores (n);

  Footprint footprint = Evaluator::footprint(params);
  uint f = 0;

  ind.stats["stime"] = 0;

  for (uint i=0; i<n; i++) {
    Simulation simulation;
    Scenario scenario (simulation);

    auto s_params = params.scenarioParams(i);
    s_params.genome = ind.dna;
    scenario.init(s_params);
    auto pstr = Params::toString(s_params);

    const Critter *emitter = scenario.emitter();
    brainless = emitter->brain().empty();

    if (i == 0) { // save subject specifics at the first evaluation
      ind.stats["brain"] = !brainless;

      const phenotype::ANN &b = emitter->brain();
      ind.stats["neurons"] = b.neurons().size()
                             - b.inputs().size()
                             - b.outputs().size();
      ind.stats["cxts"] = b.stats().edges;
    }

    /// Modular ANN
//    if (!logsSavePrefix.empty() && !annTagsFile.empty()) {
//      for (simu::Critter *c: scenario.teams()[0]) {
//        auto &brain = c->brain();
//        applyNeuralFlags(brain, annTagsFile);
//        manns.push_back(std::make_unique<phenotype::ModularANN>(brain));
//      }
//    }
  //  scenario.applyLesions(lesion);

    LogData log;
    if (!logsSavePrefix.empty())
      logging_init(&log, logsSavePrefix / Params::toString(s_params), scenario);

    auto start_time = Simulation::now();

    std::vector<float> emission;
    emission.reserve(config::Simulation::ticksPerSecond());

    while (!simulation.finished() && !aborted && !brainless) {
      simulation.step();

      if (emission.size() < emission.capacity())
        emission.push_back(scenario.emitter()->producedSound()[1]);

      // Update modules values (if modular ann is used)
      for (auto &mann: manns)
        for (const auto &p: mann->modules())
          p.second->update();

      if (!logsSavePrefix.empty()) logging_step(&log, scenario);
    }

    scores[i] = scenario.score();
    ind.stats["lg_" + pstr] = scores[i];
    mute |= scenario.mute();


    auto ee = scenario.emitter()->energyCosts,
         er = scenario.receiver()->energyCosts;

    if (i==0) footprint[f++] = ee[3]; // axons cost
    footprint[f++] = ee[2]; // emitter neural consumption
    footprint[f++] = er[2]; // receiver neural consumption
    footprint[f++] = ee[1]; // emitter vocal consumption
    footprint[f++] = er[0]; // receiver motor consumption

    // If aborted too quickly
    for (uint j=emission.size(); j<emission.capacity(); j++)
      emission.push_back(0);
    // Vocal pattern over 1st second (emitter)
    for (uint j=0; j<emission.size(); j++) footprint[f++] = emission[j];

    ind.stats["wtime"] += Simulation::durationFrom(start_time);
    ind.stats["stime"] += Scenario::DURATION - duration(simulation);
  }

  if (brainless || mute)
    std::replace(footprint.begin(), footprint.end(), NAN, 0.f);
  else if (f != footprint.size())
    utils::Thrower("Mismatch between allocated (",
                   footprint.size(), ") and used (", f, ") footprint size");

  ind.signature = footprint;

  float totalScore = 0;
  for (float score: scores) totalScore += score;
  ind.fitnesses["lg"] = totalScore / n;

  if (n > 1)
    ind.stats["stime"] = float(ind.stats["stime"]) / n;
  ind.stats["mute"] = mute;
}

void Evaluator::dumpStats(const stdfs::path &dna,
                          const stdfs::path &folder) {
//  Simulation simulation;
//  Scenario scenario (simulation, 1);
//  Team dna_ = fromJsonFile(dna).dna;
//  scenario.init({dna_, dna_, Scenario::Params::Flags(), false, ""});

//  const simu::Critter *c = scenario.subject();
//  std::ofstream ofs (folder / "stats");
//  ofs << "bdy-mass: " << c->mass() << "\n";

//  float am = 0;
//  for (const b2Body *a: c->arms()) am += a->GetMass();
//  ofs << "arm-mass: " << am << "\n";

//  const genotype::Critter &g = c->genotype();
//  static const auto deg = [] (auto rad) { return 180 * rad / M_PI; };
//  ofs << "ang-arm0: " << deg(g.splines[0].data[genotype::Spline::SA]) << "\n";
//  ofs << "ang-arm1: " << deg(g.splines[1].data[genotype::Spline::SA]) << "\n";
//  ofs << "ang-spl2: " << deg(g.splines[2].data[genotype::Spline::SA]) << "\n";
//  ofs << "ang-spl3: " << deg(g.splines[3].data[genotype::Spline::SA]) << "\n";

//  ofs << "viw-angl: " << deg(g.vision.angleBody) << "\n";
//  ofs << "viw-prec: " << g.vision.precision << "\n";
//  ofs << "viw-widt: " << deg(g.vision.width) << "\n";

//  ofs << "clk-uppr: " << g.maxClockSpeed << "\n";
//  ofs << "clk-lowr: " << g.minClockSpeed << "\n";

//  const phenotype::ANN &b = c->brain();
//  auto nn = b.neurons().size();
//  ofs << "ann-size: " << nn << "\n";
//  ofs << "ann-hidn: " << nn - b.inputsCount() - b.outputsCount() << "\n";

//  const auto &st = b.stats();
//  ofs << "ann-dpth: " << st.depth << "\n";
//  ofs << "ann-cnxt: " << st.edges << "\n";
//  ofs << "ann-lngt: " << st.axons << "\n";

}

Evaluator::Ind Evaluator::fromJsonFile(const std::string &path) {
  std::ifstream t(path);
  if (!t) utils::Thrower("Error while opening ", path);

  std::stringstream buffer;
  buffer << t.rdbuf();

  auto o = nlohmann::json::parse(buffer.str());
  if (o.count("dna")) // assuming this is a gaga individual
    return Ind(o);
  else
    return Ind(Genome(o));
}

} // end of namespace simu
