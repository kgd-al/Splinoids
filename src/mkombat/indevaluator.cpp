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
  std::ofstream lhs, rhs;
};

Evaluator::LogData* Evaluator::logging_getData(void) {
  return new Evaluator::LogData;
}

void Evaluator::logging_freeData(LogData **d) {
  delete *d;
  *d = nullptr;
}

void Evaluator::logging_init(LogData *d, const stdfs::path &f) {
  stdfs::create_directories(f);
  std::cerr << f << " should exist!\n";

  static const auto header = [] (auto &os) {
    os << "Time Count LSpeed ASpeed Health TotalHealth\n";
  };

  d->lhs.open(f / "lhs.dat");
  header(d->lhs);

  d->rhs.open(f / "rhs.dat");
  header(d->rhs);
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
       << "\n";
  };
  auto t = duration(s.simulation());
  if (d->lhs.is_open()) log(d->lhs, teams[0], t);
  if (d->rhs.is_open()) log(d->rhs, teams[1], t);
}

// ===

void Evaluator::operator() (Ind &lhs_i, const Champs &champs) {
  std::array<bool,2> brainless;

  const Team &lhs = lhs_i.dna;

  uint nChamps = champs.size();
  lhs_i.fitnesses["mk"] = 0;

  uint c = 1;
  for (const auto &rhs_i: champs) {
    const Team &rhs = rhs_i.dna;

//  using utils::operator<<;
//  for (int lesion: lesions) {

    Simulation simulation;
    Scenario scenario (simulation, lhs.size());
    Footprint footprint;

    scenario.init(lhs, rhs);

    brainless = scenario.brainless();
    bool bothBrainless = brainless[0] && brainless[1];

    uint f = 0;
    const auto t0_avg = [&scenario] (auto f, auto... args) {
      return simu::avg(scenario.teams()[0], f, args...);
    };
    using SSide = Critter::Side;
    for (SSide s: {SSide::LEFT, SSide::RIGHT})
      for (uint i=0; i<Critter::SPLINES_COUNT; i++)
        footprint[f++] = t0_avg(&Critter::splineHealth, i, s);

/// TODO Modular ANN (not implemented for 3d)
//    std::unique_ptr<phenotype::ModularANN> mann;
//    if (!logsSavePrefix.empty() && !annTagsFile.empty()) {
//      applyNeuralFlags(scenario.subject()->brain(), annTagsFile);
//      mann = std::make_unique<phenotype::ModularANN>(brain);
//    }
//    scenario.applyLesions(lesion);

    LogData log;
    if (!logsSavePrefix.empty())  logging_init(&log, logsSavePrefix);

    while (!simulation.finished() && !aborted && !bothBrainless) {
      simulation.step();

//      // Update modules values (if modular ann is used)
//      if (mann) for (const auto &p: mann->modules()) p.second->update();

      if (!logsSavePrefix.empty()) logging_step(&log, scenario);
    }

    lhs_i.fitnesses["mk"] += c * scenario.score();
//    lhs_i.stats["t"] = Scenario::DURATION - duration(simulation);
    lhs_i.stats["b"] = !brainless[0];

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

    lhs_i.infos = id(rhs_i);

//    assert(!brainless[0] && !brainless[1]);
    c++;
  }

  lhs_i.fitnesses["mk"] /= nChamps * (nChamps+1) / 2.f;
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
                                   const std::string &rhsFile) {
  static const std::regex regex
    (".*ID([0-9]+)/([AB])/gen([0-9]+)/.*_([0-9]+)\\.dna");

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

  return oss.str();
}

} // end of namespace simu
