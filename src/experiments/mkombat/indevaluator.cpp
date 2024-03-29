#include "indevaluator.h"

namespace simu {

std::atomic<bool> Evaluator::aborted = false;

Evaluator::Evaluator (void) {
//  lesions = {0};
}

//void Evaluator::setLesionTypes(const std::string &s) {
//  if (s.empty())
//    return;

//  else if (s == "all")
//    lesions = {0,1,2,3};

//  else {
//    for (const std::string str: utils::split(s, ';')) {
//      int i;
//      if (std::istringstream (str) >> i)
//        lesions.push_back(i);
//    }
//  }
//}

void Evaluator::applyNeuralFlags(phenotype::ANN &ann,
                                 const std::string &tagsfile) {
  auto &n = ann.neurons();

  std::ifstream ifs (tagsfile);
  if (!ifs)
    utils::Thrower("Failed to open neural tags file '", tagsfile, "'");

  for (std::string line; std::getline(ifs, line); ) {
    if (line.empty() || line[0] == '/') continue;
    std::istringstream iss (line);
    phenotype::Point pos;
    iss >> pos;

    auto it = n.find(pos);
    if (it == n.end())
      utils::Thrower("No neuron at position {", pos.x(), ", ", pos.y(), "}");

    iss >> (*it)->flags;
  }

  // Also aggregate similar inputs/outputs
  for (auto &p: ann.neurons()) {
    using P = phenotype::Point;
    phenotype::ANN::Neuron &n = *p;
    P pos = n.pos;
    auto &f = n.flags;

    if (n.type == phenotype::ANN::Neuron::I) {
      if (pos == P{0.f, -1.f, -.5f} || pos == P{0.f, -1.f, -1.f})
        f = 1<<18;  // health
      else if (pos.z() <= -1/3.) {
        f = 1<<19;  // audition
        if (pos.x() > 0)  f |= 1<<17;  // right
      } else if (pos.z() >= 1/3.) {
        f = 1<<20;  // vision
        if (pos.x() > 0)  f |= 1<<17;  // right
      } else {
        f = 1<<21;  // touch
        if (pos.x() > 0)  f |= 1<<17;  // right
      }
      if (f) f |= 1<<16; // input

    } else if (n.type == phenotype::ANN::Neuron::O) {
      if (pos.z() >= .5)
        f = 1 << 26;  // Voice
      else if (pos.z() == -.5f) {
        f = 1<<27;    // arms
        if (pos.x() > 0) f |= 1<<25;  // right
      }
      if (f) f |= 1<<24; // output
    }
  }

  if (config::Simulation::verbosity() > 0) {
    std::cout << "Neural flags:\n";
    for (const auto &p: n)
      std::cout << "\t" << std::setfill(' ') << std::setw(10) << p->pos
                << ":\t"
                << std::bitset<8*sizeof(p->flags)>(p->flags)
                << "\n";
  }
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
  std::ofstream lhs, rhs, // generic individual data
                fdata, adata, // fight / accoustics
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

  static const auto header = [] (auto &os) {
    os << "Time Count LSpeed ASpeed Health TotalHealth Energy\n";
  };

  uint team0Size = s.teams()[0].size();
  const auto filename = [&team0Size] (const std::string &name,
                                      const std::string &ext,
                                      uint i) {
    std::string id;
    if (team0Size > 1) id = utils::mergeToString("_", i);
    return utils::mergeToString(name, id, ".", ext);
  };

  d->lhs.open(f / "lhs.dat");
  header(d->lhs);

  d->rhs.open(f / "rhs.dat");
  header(d->rhs);

  d->fdata.open(f / "impacts.dat");
  d->fdata << "Time A B dVA dVB D aA aB\n";

  d->adata.open(f / "acoustics.dat");
  auto &alog = d->adata;
  for (uint i=0; i<s.teams()[0].size(); i++) {
    std::string prefix;
    if (s.teams()[0].size() > 1) prefix = utils::mergeToString("I", i);
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

  d->idata.resize(team0Size);
  d->odata.resize(team0Size);
  for (uint i=0; i<team0Size; i++) {
    auto &ilog = d->idata[i];
    ilog.open(f / filename("inputs", "dat", i));
    for (auto s: s.subject()->neuralInputsHeader()) ilog << s << " ";
    ilog << "\n";

    auto &olog = d->odata[i];
    olog.open(f / filename("outputs", "dat", i));
    for (auto s: s.subject()->neuralOutputsHeader()) olog << s << " ";
    olog << "\n";
  }

  if (s.neuralEvaluation() && annTagsFile.empty()) {
    auto &nlog = d->ndata;
    nlog.open(f / "neurons.dat");
    nlog << NEURAL_FLAGS;
    for (const auto &p: s.subject()->brain().neurons())
      if (p->isHidden())
        nlog << " (" << p->pos << ")";
    nlog << "\n";
  }

  if (!annTagsFile.empty()) {
    auto &mlogs = d->mdata;
    mlogs.resize(team0Size);
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
    const phenotype::ANN &b = s.subject()->brain();
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
  auto teams = s.teams();
  static const auto log = [] (auto &os, auto team, auto time) {
    const auto avg = [&team] (auto f) { return simu::avg(team, f); };
    os << time << " " << team.size() << " "
       << " " << avg(&Critter::linearSpeed)
       << " " << avg(&Critter::angularSpeed)
       << " " << avg(&Critter::bodyHealthness)
       << " " << avg(&Critter::healthness)
       << " " << avg(&Critter::usableEnergy)
       << "\n";
  };
  auto t = duration(s.simulation());
  if (d->lhs.is_open()) log(d->lhs, teams[0], t);
  if (d->rhs.is_open()) log(d->rhs, teams[1], t);

  if (teams[0].empty()) return;
  uint team0Size = s.teams()[0].size();
  const auto teammate = [&s] (uint i) {
    auto it = s.teams()[0].begin();
    std::advance(it, i);
    return *it;
  };

  if (d->fdata.is_open()) {
    auto timestamp = s.simulation().currTime().timestamp();
    std::string event;
    std::istringstream l (s.simulation().environment().fightDataLogger.str());
    while (std::getline(l, event))
      d->fdata << timestamp << " " << event << "\n";
  }

  if (d->adata.is_open()) {
    auto &alog = d->adata;
    for (const simu::Critter *c: s.teams()[0]) {
      for (auto v: c->ears()) alog << v << " ";
      for (auto v: c->producedSound()) alog << v << " ";
    }
    alog << "\n";
  }

  for (uint i=0; i<team0Size; i++) {
    auto &ilog = d->idata[i];
    if (ilog.is_open()) {
      for (const auto &v: teammate(i)->neuralInputs()) ilog << v << " ";
      ilog << "\n";
    }

    auto &olog = d->odata[i];
    if (olog.is_open()) {
      for (const auto &v: teammate(i)->neuralOutputs()) olog << v << " ";
      olog << "\n";
    }
  }

  if (s.neuralEvaluation()) {
    auto &os = d->ndata;
    os << s.currentFlags();
    for (const auto &p: s.subject()->brain().neurons())
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

Evaluator::Params Evaluator::Params::fromArgv (
    const std::string &lhsArg, const std::vector<std::string> &rhsArgs,
    const std::string &scenario, const std::string &scenarioArg,
    int teamSize) {

  Evaluator::Params params (fromJsonFile(lhsArg));
  params.scenario = scenario;

  if (teamSize == -1) {
    teamSize = params.ind.dna.size;
    for (Ind &i: params.opps)
      if (teamSize != int(i.dna.size))
        utils::Thrower("Provided genomes have different preferred team sizes."
                       " Force one through the --team-size=S option");
  }
  params.teamSize = teamSize;

  if (neuralEvaluation(scenario)) {
    params.flags.reset();

    using F = simu::Scenario::Params::Flag;
    std::string s = scenario;
    if (s.size() != 4) utils::Thrower("Scenario '", s, "' has unexpected size");

    bool ok = true;
    switch (s[1]) {
    case '1':
      switch (s[3]) {
      case 'a': params.flags.set(F::PAIN_ABSL); break;
      case 'i': params.flags.set(F::PAIN_INST); break;
      case 't': params.flags.set(F::TOUCH); break;
      default: ok = false;
      }
      break;

    case '2':
      switch (s[3]) {
      case 'a': params.flags.set(F::WITH_ALLY); break;
      case 'b': params.flags.set(F::WITH_OPP1); break;
      case 'c': params.flags.set(F::WITH_OPP2); break;
      default: ok = false;
      }
      break;

    case '3':
      switch (s[3]) {
      case 'n': params.flags.set(F::SOUND_NOIS); break;
      case 'f': params.flags.set(F::SOUND_FRND); break;
      case 'o': params.flags.set(F::SOUND_OPPN); break;
      default: ok = false;
      }
      if (scenarioArg.empty() && s[3] != 'n')
        utils::Thrower("Type 3 scenario requires an additional argument");
      else
        params.scenarioArg = scenarioArg;
      break;

    default: ok = false;
    }

    if (!params.flags.any() || !ok)
      utils::Thrower("Invalid scenario '", scenario, "'");
    assert(params.flags.any() && ok);
  }

  for (uint i=0; i<rhsArgs.size(); i++) {
    params.opps.push_back(fromJsonFile(rhsArgs[i]));
    params.oppsId.push_back(id(params.opps.back()));
    params.kombatNames.push_back(kombatName(lhsArg, rhsArgs[i], scenario));
  }

  return params;
}

Evaluator::Params Evaluator::Params::fromInds(Ind &ind, const Inds &opps) {
  Evaluator::Params params (ind);
  for (uint i=0; i<opps.size(); i++) params.opps.push_back(opps[i]);
  params.scenario = "mk";
  params.teamSize = ind.dna.size;
  params.flags.reset();
  return params;
}

Scenario::Params Evaluator::Params::scenarioParams(uint i) const {
  Scenario::Params params;
  params.lhs = ind.dna;
  params.rhs = opps[i].dna;
  params.flags = flags;
  params.neutralFirst = false;
  params.arg = scenarioArg;
  return params;
}

std::string Evaluator::Params::opponentsIds(void) const {
  std::string ids = id(opps[0]);
  for (uint i=1; i<opps.size(); i++)
    ids += "/" + id(opps[i]);
  return ids;
}

// ===

bool Evaluator::neuralEvaluation(const std::string &scenario) {
  return !scenario.empty() && scenario != "mk";
}

std::string Evaluator::kombatName (const std::string &lhsFile,
                                   const std::string &rhsFile,
                                   const std::string &scenario) {
  static const std::regex regex
    (".*ID(\\w+)/([A-Z])/gen(\\d+)/.*_(\\d+)\\.dna");

  std::ostringstream oss;
  const auto merge = [&oss] (std::smatch tokens) {
    if (tokens.size() == 5) {
      oss << tokens[1];
      for (uint i = 2; i < tokens.size(); i++)
        oss << "-" << tokens[i];
    } else
      oss << "PARSE_ERROR";
  };

  std::smatch pieces;
  std::regex_match(lhsFile, pieces, regex);
  merge(pieces);

  oss << "__";

  std::regex_match(rhsFile, pieces, regex);
  merge(pieces);

  if (neuralEvaluation(scenario))
    oss << "__" << scenario;

  return oss.str();
}

// ===

void Evaluator::operator () (Ind &ind, const Inds &opps) {
  Params params = Params::fromInds(ind, opps);
  operator() (params);
  ind = params.ind;
  ind.infos = params.opponentsIds();
}

uint Evaluator::footprintSize(uint evaluations) {
  static constexpr auto NS = 2*Critter::SPLINES_COUNT;
  return NS             // splines health at start
       + 2              // mass and moment of inertia
       + evaluations    // after each evaluation collect:
         * (
              1         // > body health
            + NS        // > splines health
            + 2         // > final position
         );
}

std::vector<std::string> Evaluator::footprintFields (uint evaluations) {
  std::vector<std::string> v (footprintSize(evaluations));
  uint f = 0;

  using SSide = Critter::Side;
  for (SSide s: {SSide::LEFT, SSide::RIGHT})
    for (uint i=0; i<Critter::SPLINES_COUNT; i++)
      v[f++] = utils::mergeToString("SH1", uint(s), i);
  v[f++] = "Mass";
  v[f++] = "MoI";

  for (uint i=0; i<evaluations; i++) {
    auto prefix = utils::mergeToString("E", i);
    v[f++] = prefix + "BH1";
    for (SSide s: {SSide::LEFT, SSide::RIGHT})
      for (uint i=0; i<Critter::SPLINES_COUNT; i++)
        v[f++] = utils::mergeToString(prefix, "SH1", uint(s), i);
    v[f++] = prefix + "X";
    v[f++] = prefix + "Y";
  }

  return v;
}

void Evaluator::operator() (Params &params) {
  std::array<bool,2> brainless;

//  using utils::operator<<;
//  for (int lesion: lesions) {

  Ind &ind = params.ind;
  const uint n = params.opps.size();
  std::vector<float> scores (n);
  Footprint footprint (footprintSize(n));

  ind.stats["stime"] = 0;

  uint f = 0;
  for (uint i=0; i<n; i++) {
    Simulation simulation;
    Scenario scenario (simulation, params.teamSize);

    scenario.init(params.scenarioParams(i));

    brainless = scenario.brainless();

    const auto t0_avg = [&scenario] (auto f, auto... args) {
      return simu::avg(scenario.teams()[0], f, args...);
    };
    using SSide = Critter::Side;
    if (i == 0) { // save subject specifics at the first evaluation
      ind.stats["brain"] = !brainless[0];

      phenotype::ANN &b = scenario.subject()->brain();
      ind.stats["neurons"] = b.neurons().size()
                             - b.inputs().size()
                             - b.outputs().size();
      ind.stats["cxts"] = b.stats().edges;

      for (SSide s: {SSide::LEFT, SSide::RIGHT})
        for (uint j=0; j<Critter::SPLINES_COUNT; j++)
          footprint[f++] = t0_avg(&Critter::splineHealth, j, s);
      footprint[f++] = t0_avg(&Critter::mass);
      footprint[f++] = t0_avg(&Critter::momentOfInertia);
    }

    /// Modular ANN
    if (!logsSavePrefix.empty() && !annTagsFile.empty()) {
      for (simu::Critter *c: scenario.teams()[0]) {
        auto &brain = c->brain();
        applyNeuralFlags(brain, annTagsFile);
        manns.push_back(std::make_unique<phenotype::ModularANN>(brain));
      }
    }
  //  scenario.applyLesions(lesion);

    LogData log;
    if (!logsSavePrefix.empty())
      logging_init(&log, logsSavePrefix / params.kombatNames[i], scenario);

    auto start_time = Simulation::now();

    while (!simulation.finished() && !aborted) {
      simulation.step();

  //    // Update modules values (if modular ann is used)
      for (auto &mann: manns)
        for (const auto &p: mann->modules())
          p.second->update();

      if (!logsSavePrefix.empty()) logging_step(&log, scenario);
    }

    scores[i] = scenario.score();

    if (n > 1) ind.stats[utils::mergeToString("mk", i)] = scores[i];

    ind.stats["wtime"] += Simulation::durationFrom(start_time);
    ind.stats["stime"] += Scenario::DURATION - duration(simulation);

//    const auto &autopsies = scenario.autopsies();
//    lhs_i.stats["injury"] = autopsies[1][Scenario::DeathCause::INJURY];
//    lhs_i.stats["starvation"] = 1-autopsies[1][Scenario::DeathCause::STARVATION];

    footprint[f++] = t0_avg(&Critter::bodyHealth);
    for (SSide s: {SSide::LEFT, SSide::RIGHT})
      for (uint i=0; i<Critter::SPLINES_COUNT; i++)
        footprint[f++] = t0_avg(&Critter::splineHealth, i, s);
    footprint[f++] = t0_avg(&Critter::x);
    footprint[f++] = t0_avg(&Critter::y);
  }

  assert(f == footprintSize(n));
  ind.signature = footprint;

  if (n == 1) {
    ind.fitnesses["mk"] = scores[0];

  } else if (n == 2) {
    ind.fitnesses["mk"] =
        2.f * std::min(scores[0], scores[1]) / 3.f
      + 1.f * std::max(scores[0], scores[1]) / 3.f;
  } else
    utils::Thrower("mk fitness not defined for n = ", n, " > 2");

  if (n > 1) {
    ind.stats["stime"] = float(ind.stats["stime"]) / n;
  }
}

void Evaluator::dumpStats(const stdfs::path &dna,
                          const stdfs::path &folder) {
  Simulation simulation;
  Scenario scenario (simulation, 1);
  Team dna_ = fromJsonFile(dna).dna;
  scenario.init({dna_, dna_, Scenario::Params::Flags(), false, ""});

  const simu::Critter *c = scenario.subject();
  std::ofstream ofs (folder / "stats");
  ofs << "bdy-mass: " << c->mass() << "\n";

  float am = 0;
  for (const b2Body *a: c->arms()) am += a->GetMass();
  ofs << "arm-mass: " << am << "\n";

  const genotype::Critter &g = c->genotype();
  static const auto deg = [] (auto rad) { return 180 * rad / M_PI; };
  ofs << "ang-arm0: " << deg(g.splines[0].data[genotype::Spline::SA]) << "\n";
  ofs << "ang-arm1: " << deg(g.splines[1].data[genotype::Spline::SA]) << "\n";
  ofs << "ang-spl2: " << deg(g.splines[2].data[genotype::Spline::SA]) << "\n";
  ofs << "ang-spl3: " << deg(g.splines[3].data[genotype::Spline::SA]) << "\n";

  ofs << "viw-angl: " << deg(g.vision.angleBody) << "\n";
  ofs << "viw-prec: " << g.vision.precision << "\n";
  ofs << "viw-widt: " << deg(g.vision.width) << "\n";

  ofs << "clk-uppr: " << g.maxClockSpeed << "\n";
  ofs << "clk-lowr: " << g.minClockSpeed << "\n";

  const phenotype::ANN &b = c->brain();
  auto nn = b.neurons().size();
  ofs << "ann-size: " << nn << "\n";
  ofs << "ann-hidn: " << nn - b.inputsCount() - b.outputsCount() << "\n";

  const auto &st = b.stats();
  ofs << "ann-dpth: " << st.depth << "\n";
  ofs << "ann-cnxt: " << st.edges << "\n";
  ofs << "ann-lngt: " << st.axons << "\n";

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
