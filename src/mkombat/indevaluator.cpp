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
  std::ofstream lhs, rhs, adata, ndata;
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

  static const auto header = [] (auto &os) {
    os << "Time Count LSpeed ASpeed Health TotalHealth Energy\n";
  };

  d->lhs.open(f / "lhs.dat");
  header(d->lhs);

  d->rhs.open(f / "rhs.dat");
  header(d->rhs);

  d->adata.open(f / "acoustics.dat");
  auto &alog = d->adata;
  for (auto s: {'L', 'R'}) {
    alog << "I" << s << "N ";
    for (uint i=0; i<Critter::VOCAL_CHANNELS; i++)
      alog << "I" << s << i << " ";
  }
  alog << "ON";
  for (uint i=0; i<Critter::VOCAL_CHANNELS; i++)
    alog << " O" << i;
  alog << "\n";

  if (s.neuralEvaluation()) {
    auto &nlog = d->ndata;
    nlog.open(f / "neurons.dat");
    nlog << "AOS";
    for (const auto &p: s.subject()->brain().neurons())
      if (p->isHidden())
        nlog << " (" << p->pos << ")";
    nlog << "\n";
  }

  {
    std::ofstream blog (f / "brain.dat");
    const auto &b = s.subject()->brain();
    blog << "Type X Y Z Depth Flags\n";
    for (const phenotype::ANN::Neuron::ptr &p: b.neurons()) {
      blog << p->type
           << " " << p->pos.x() << " " << p->pos.y() << " " << p->pos.z()
           << " " << p->depth << " " << p->flags << "\n";
    }
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

  if (d->adata.is_open()) {
    auto &alog = d->adata;
    const auto &sbj = s.subject();
    for (auto v: sbj->ears()) alog << v << " ";
    for (auto v: sbj->producedSound()) alog << v << " ";
    alog << "\n";
  }

  if (s.neuralEvaluation()) {
    auto &os = d->ndata;
    os << s.currentFlags();
    for (const auto &p: s.subject()->brain().neurons())
      if (p->isHidden())
        os << " " << p->value;
    os << "\n";
  }
}

// ===

const std::set<std::string> Evaluator::canonicalScenarios {
  "pain", // Damage simulation
  "vind"  // Inflicted damage
};

Evaluator::Params Evaluator::Params::fromArgv (
    const std::string &lhsArg, const std::vector<std::string> &rhsArgs,
    const std::string &scenario, int teamSize) {

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
    if (s == "self") params.flags.set(F::PAIN_SELF);
    if (s == "othr") params.flags.set(F::PAIN_OTHER);
    if (s == "ally") params.flags.set(F::PAIN_ALLY);
    assert(params.flags.any());
    if (!params.flags.any())
      utils::Thrower("Invalid scenario '", scenario, "'");
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
  return Scenario::Params { ind.dna, opps[i].dna, flags };
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
    std::unique_ptr<phenotype::ModularANN> mann;
    if (!logsSavePrefix.empty() && !annTagsFile.empty()) {
      auto &brain = scenario.subject()->brain();
      applyNeuralFlags(brain, annTagsFile);
      mann = std::make_unique<phenotype::ModularANN>(brain);
    }
  //  scenario.applyLesions(lesion);

    LogData log;
    if (!logsSavePrefix.empty())
      logging_init(&log, logsSavePrefix / params.kombatNames[i], scenario);

    auto start_time = Simulation::now();

    while (!simulation.finished() && !aborted) {
      simulation.step();

  //    // Update modules values (if modular ann is used)
      if (mann) for (const auto &p: mann->modules()) p.second->update();

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
