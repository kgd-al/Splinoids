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

void Evaluator::operator() (Team &lhs, Team &rhs) {
  std::array<bool,2> brainless;

  using utils::operator<<;
  for (int lesion: lesions) {

    Simulation simulation;
    Scenario scenario (simulation, lhs.size());

    scenario.init(lhs, rhs);

    brainless = scenario.brainless();
//    if (brainless[0] || brainless[1])  break;

/// TODO Modular ANN (not implemented for 3d)
//    std::unique_ptr<phenotype::ModularANN> mann;
//    if (!logsSavePrefix.empty() && !annTagsFile.empty()) {
//      applyNeuralFlags(scenario.subject()->brain(), annTagsFile);
//      mann = std::make_unique<phenotype::ModularANN>(brain);
//    }
//    scenario.applyLesions(lesion);

/// TODO Log (what to log?)
    std::ofstream llog, rlog;
    if (!logsSavePrefix.empty()) {

      stdfs::create_directories(logsSavePrefix);
      std::cerr << logsSavePrefix << " should exist!\n";

      static const auto header = [] (auto &os) {
        os << "Time Count LSpeed ASpeed Health TotalHealth\n";
      };

      llog.open(logsSavePrefix / "lhs.dat");
      header(llog);

      rlog.open(logsSavePrefix / "rhs.dat");
      header(rlog);
    }

    while (!simulation.finished() && !aborted) {
      simulation.step();

//      // Update modules values (if modular ann is used)
//      if (mann) for (const auto &p: mann->modules()) p.second->update();

      if (!logsSavePrefix.empty()) {
        auto teams = scenario.teams();
        static const auto log = [] (auto &os, auto team, auto time) {
          auto s = team.size();
          os << time << " " << s << " ";

          if (s > 0) {
            float avgLSpeed = 0, avgASpeed = 0, avgHealth = 0, avgTHealth = 0;
            for (const simu::Critter *c: team) {
              avgLSpeed += c->linearSpeed();
              avgASpeed += std::fabs(c->angularSpeed());
              avgHealth += c->bodyHealthness();
              avgTHealth += c->healthness();
            }
            avgLSpeed /= s;
            avgASpeed /= s;
            avgHealth /= s;
            avgTHealth /= s;

            os << " " << avgLSpeed << " " << avgASpeed
               << " " << avgHealth << " " << avgTHealth;
          } else
            os << "0 0 0 0";

          os << "\n";
        };
        auto t = float(simulation.currTime().timestamp())
            / config::Simulation::ticksPerSecond();
        if (llog.is_open()) log(llog, teams[0], t);
        if (rlog.is_open()) log(rlog, teams[1], t);
      }
    }

    auto scores = scenario.scores();
    lhs.fitness = scores[0];
    rhs.fitness = scores[1];
//    assert(!brainless[0] && !brainless[1]);
  }
}

//Evaluator::Ind Evaluator::fromJsonFile(const std::string &path) {
//  std::ifstream t(path);
//  std::stringstream buffer;
//  buffer << t.rdbuf();
//  auto o = nlohmann::json::parse(buffer.str());

//  if (o.count("dna")) // assuming this is a gaga individual
//    return Ind(o);
//  else {
//    DNA g = o;
//    return Ind(g);
//  }
//}

std::string Evaluator::kombatName (const std::string &lhsFile,
                                   const std::string &rhsFile) {
  const std::regex regex (".*ID([0-9]+).*[^[:alnum:]](\\w+)\\.team\\.json");
  std::ostringstream oss;
  std::smatch pieces;
  std::regex_match(lhsFile, pieces, regex);
  if (pieces.size() == 3)
    oss << pieces[1] << "_" << pieces[2];
  else
    oss << "PARSE_ERROR";
  oss << "__";
  std::regex_match(rhsFile, pieces, regex);
  if (pieces.size() == 3)
    oss << pieces[1] << "_" << pieces[2];
  else
    oss << "PARSE_ERROR";
  return oss.str();
}

} // end of namespace simu
