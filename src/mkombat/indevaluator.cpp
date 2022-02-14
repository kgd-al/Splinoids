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

//void Evaluator::applyNeuralFlags(phenotype::ANN &ann,
//                                    const std::string &tagsfile) {
//  auto &n = ann.neurons();

//  std::ifstream ifs (tagsfile);
//  if (!ifs)
//    utils::doThrow<std::invalid_argument>(
//      "Failed to open neural tags file '", tagsfile, "'");

//  for (std::string line; std::getline(ifs, line); ) {
//    if (line.empty() || line[0] == '/') continue;
//    std::istringstream iss (line);
//    phenotype::Point pos;
//    iss >> pos;

//    auto it = n.find(pos);
//    if (it == n.end())
//      utils::doThrow<std::invalid_argument>(
//        "No neuron at position {", pos.x(), ", ", pos.y(), "}");

//    iss >> (*it)->flags;
//  }

//  if (config::Simulation::verbosity() > 0) {
//    std::cout << "Neural flags:\n";
//    for (const auto &p: n)
//      std::cout << "\t" << std::setfill(' ') << std::setw(10) << p->pos
//                << ":\t"
//                << std::bitset<8*sizeof(p->flags)>(p->flags)
//                << "\n";
//  }
//}

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
  std::ofstream lhs, rhs, ndata;
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

  if (s.neuralEvaluation()) {
    auto &os = d->ndata;
    os.open(f / "neurons.dat");
    os << "IAO";
    for (const auto &p: s.subject()->brain().neurons())
      if (p->isHidden())
        os << " (" << p->pos.x() << "," << p->pos.y() << ")";
    os << "\n";
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

Scenario::Params Evaluator::scenarioFromStrings(
    const std::string &lhsArg, const std::string &rhsArg) {
  Scenario::Params params;
  params.desc = rhsArg;
  params.neuralEvaluation = (rhsArg[0] == ':');
  if (!params.neuralEvaluation) {
    auto rhs = fromJsonFile(rhsArg);
    params.rhs = rhs.dna;
    params.rhsId = simu::Evaluator::id(rhs);
    params.flags.reset();

  } else {
    params.flags.reset();

    using F = simu::Scenario::Params::Flag;
    std::string s = rhsArg.substr(1);
    if (s == "test") params.flags.set(F::TEST);
    if (s == "pain_i" || s == "pain_b") params.flags.set(F::PAIN_INSTANTANEOUS);
    if (s == "pain_a" || s == "pain_b") params.flags.set(F::PAIN_ABSOLUTE);
    if (s == "vind") params.flags.set(F::PAIN_OPPONENT);
    assert(params.flags.any());
  }

  params.kombatName = kombatName(lhsArg, rhsArg);

  return params;
}

void Evaluator::operator () (Ind &lhs, const Ind &rhs) {
  Scenario::Params params;
  params.desc = ":mk";
  params.rhs = rhs.dna;
  params.neuralEvaluation = false;
  params.flags.reset();
  operator() (lhs, params);
  lhs.infos = id(rhs);
}

const std::set<std::string> Evaluator::canonicalScenarios {
  "pain_i", // Instantaneous pain
  "pain_a", // Health level
  "pain_b", // Damage simulation
  "vind"    // Inflicted damage
};

void Evaluator::operator() (Ind &lhs_i, const Scenario::Params &params) {
  std::array<bool,2> brainless;

//  using utils::operator<<;
//  for (int lesion: lesions) {

  Simulation simulation;
  Scenario scenario (simulation, lhs_i.dna.size());
  Footprint footprint;

  scenario.init(lhs_i.dna, params);

  brainless = scenario.brainless();
  lhs_i.stats["brain"] = !brainless[0];
  bool bothBrainless = brainless[0] && brainless[1];

  {
    phenotype::ANN &b = scenario.subject()->brain();
    lhs_i.stats["neurons"] = b.neurons().size()
                           - b.inputs().size()
                           - b.outputs().size();
    lhs_i.stats["cxts"] = b.stats().edges;
  }

  uint f = 0;
  const auto t0_avg = [&scenario] (auto f, auto... args) {
    return simu::avg(scenario.teams()[0], f, args...);
  };
  using SSide = Critter::Side;
  for (SSide s: {SSide::LEFT, SSide::RIGHT})
    for (uint i=0; i<Critter::SPLINES_COUNT; i++)
      footprint[f++] = t0_avg(&Critter::splineHealth, i, s);

/// TODO Modular ANN (not implemented for 3d)
//  std::unique_ptr<phenotype::ModularANN> mann;
//  if (!logsSavePrefix.empty() && !annTagsFile.empty()) {
//    applyNeuralFlags(scenario.subject()->brain(), annTagsFile);
//    mann = std::make_unique<phenotype::ModularANN>(brain);
//  }
//  scenario.applyLesions(lesion);

  LogData log;
  if (!logsSavePrefix.empty())  logging_init(&log, logsSavePrefix, scenario);

  auto start_time = Simulation::now();
  bool fail = brainless[0];

  while (!simulation.finished() && !aborted && !fail) {
    simulation.step();

    fail |= (Simulation::durationFrom(start_time) >= 1000);

//    // Update modules values (if modular ann is used)
//    if (mann) for (const auto &p: mann->modules()) p.second->update();

    if (!logsSavePrefix.empty()) logging_step(&log, scenario);
  }

  lhs_i.fitnesses["mk"] = fail ? -4 : scenario.score();

  lhs_i.stats["wtime"] = -Simulation::durationFrom(start_time);
  lhs_i.stats["stime"] = Scenario::DURATION - duration(simulation);

//  const auto &autopsies = scenario.autopsies();
//  lhs_i.stats["injury"] = autopsies[1][Scenario::DeathCause::INJURY];
//  lhs_i.stats["starvation"] = 1-autopsies[1][Scenario::DeathCause::STARVATION];

  footprint[f++] = t0_avg(&Critter::bodyHealth);
  for (SSide s: {SSide::LEFT, SSide::RIGHT})
    for (uint i=0; i<Critter::SPLINES_COUNT; i++)
      footprint[f++] = t0_avg(&Critter::splineHealth, i, s);
  footprint[f++] = t0_avg(&Critter::mass);
  footprint[f++] = t0_avg(&Critter::momentOfInertia);
  footprint[f++] = t0_avg(&Critter::x);
  footprint[f++] = t0_avg(&Critter::y);
  lhs_i.signature = footprint;
  assert(f == FOOTPRINT_DIM);

//  std::cerr << __PRETTY_FUNCTION__ << simulation.currTime().timestamp()
//            << " " << brainless[0] << " " << brainless[1] << "\n";

//    assert(!brainless[0] && !brainless[1]);
}

Evaluator::Ind Evaluator::fromJsonFile(const std::string &path) {
  std::ifstream t(path);
  if (!t) utils::doThrow<std::invalid_argument>("Error while opening ", path);

  std::stringstream buffer;
  buffer << t.rdbuf();

  auto o = nlohmann::json::parse(buffer.str());
  if (o.count("dna")) // assuming this is a gaga individual
    return Ind(o);
  else
    return Ind(Genome(o));
}

std::string Evaluator::kombatName (const std::string &lhsFile,
                                   const std::string &rhsArg) {
  static const std::regex regex
    (".*ID(\\w+)/([AB])/gen(\\d+)/.*_(\\d+)\\.dna");

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

  if (rhsArg[0] == ':')
    oss << rhsArg.substr(1);
  else {
    std::regex_match(rhsArg, pieces, regex);
    merge(pieces);
  }

  return oss.str();
}

} // end of namespace simu
