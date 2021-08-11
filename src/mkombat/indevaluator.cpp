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
    if (brainless[0] || brainless[1])  break;

/// TODO Modular ANN (not implemented for 3d)
//    std::unique_ptr<phenotype::ModularANN> mann;
//    if (!logsSavePrefix.empty() && !annTagsFile.empty()) {
//      applyNeuralFlags(scenario.subject()->brain(), annTagsFile);
//      mann = std::make_unique<phenotype::ModularANN>(brain);
//    }
//    scenario.applyLesions(lesion);

/// TODO Log (what to log?)
//    std::bitset<5> tags;
//    std::ofstream tlog, alog, nlog;
//    std::ofstream olog, mlog;
//    if (!logsSavePrefix.empty()) {
//      stdfs::path savePath = logsSavePrefix / specStr;

//      stdfs::create_directories(savePath);

//      std::string stags = "GAENV";

//      tlog.open(savePath / "trajectory.dat");
//      tlog << "Env size: " << simulation.environment().xextent()
//           << " " << simulation.environment().yextent() << "\n"
//           << "Food_x Food_y Food_r\n";
//      if (auto f = scenario.foodlet())
//        tlog << f->x() << " " << f->y() << " " << f->radius() << "\n\n";

//      tlog << stags << " sx sy sa cx cy ca px py pa\n";

//      alog.open(savePath / "vocalisation.dat");
//      alog << "Noise";
//      for (uint i=0; i<Critter::VOCAL_CHANNELS; i++)
//        alog << " S" << i;
//      for (uint i=0; i<Critter::VOCAL_CHANNELS; i++)
//        alog << " C" << i;
//      alog << "\n";

//      nlog.open(savePath / "neurons.dat");
//      nlog << stags;
//      for (const auto &p: brain.neurons())
//        if (p->isHidden())
//          nlog << " (" << p->pos.x() << "," << p->pos.y() << ")";
//      nlog << "\n";

//      if (!annTagsFile.empty()) {
//        olog.open(savePath / "outputs.dat");
//        olog << "ML MR CS VV VC\n";

//        mlog.open(savePath / "modules.dat");
//        mlog << stags;
//        for (const auto &p: mann->modules()) {
//          if (p.second->type() == phenotype::ANN::Neuron::H) {
//            auto f = p.second->flags;
//            mlog << " " << f << "M " << f << "S";
//          }
//        }
//        mlog << "\n";

//        std::ofstream slog (savePath / "nstats");
//        std::array<uint, 3> n {0};
//        uint c = 0;
//        float s = 0;
//        for (const auto &p: brain.neurons()) {
//          n[p->type]++;
//          c += p->links().size();
//          s += p->pos.x();
//        }
//        slog << "INeurons: " << n[0] << "\n"
//             << "ONeurons: " << n[1] << "\n"
//             << "HNeurons: " << n[2] << "\n"
//             << "Connections: " << c << "\n";
//        slog << "Skewdness: " << s / brain.neurons().size() << "\n";
//        n.fill(0);
//        c = 0;
//        for (const auto &p: mann->modules()) {
//          n[p.second->type()]++;
//          c += p.second->links.size();
//        }
//        slog << "IModules: " << n[0] << "\n"
//             << "OModules: " << n[1] << "\n"
//             << "HModules: " << n[2] << "\n"
//             << "MConnections: " << c << "\n";
//      }
//    }

    while (!simulation.finished() && !aborted) {
      simulation.step();

//      // Update modules values (if modular ann is used)
//      if (mann) for (const auto &p: mann->modules()) p.second->update();

//      if (!logsSavePrefix.empty()) {
//        tags.reset();
//        const auto &r = s.retina();
//        tags[4] = std::any_of(r.begin(), r.end(),
//          [] (const auto &c) { return c[0] == 0 && c[1] > 0 && c[2] == 0; });

//        tags[3] = std::any_of(r.begin(), r.end(),
//          [] (const auto &c) { return std::all_of(c.begin(), c.end(),
//            [] (auto v) { return 0 < v && v < 1; }); });

//        tags[2] = std::any_of(r.begin(), r.end(),
//          [] (const auto &c) { return c[0] == 1; });

//        const auto &e = s.ears();
//        for (uint i=0; i<e.size(); i++) {
//          uint ti = (i%(Critter::VOCAL_CHANNELS+1) == 0) ? 1 : 0;
//          tags[ti] = tags[ti] | (e[i] > 0);
//        }
//      }

//      if (tlog.is_open()) {
//        tlog << tags
//             << " " << s.x() << " " << s.y() << " " << s.rotation() << " ";

//        if (auto c = scenario.clone())
//          tlog << c->x() << " " << c->y() << " " << c->rotation();
//        else
//          tlog << "nan nan nan";
//        tlog << " ";

//        if (auto p = scenario.predator())
//          tlog << p->x() << " " << p->y() << " " << p->rotation();
//        else
//          tlog << "nan nan nan";

//        tlog << "\n";
//      }

//      if (alog.is_open()) {
//        for (float v: s.producedSound())  alog << v << " ";

//        if (auto c = scenario.clone())
//          for (uint i=0; i<Critter::VOCAL_CHANNELS; i++)
//            alog << c->producedSound()[i+1] << " ";
//        else
//          for (uint i=0; i<Critter::VOCAL_CHANNELS; i++)
//            alog << "nan ";
//        alog << "\n";
//      }

//      if (nlog.is_open()) {
//        nlog << tags;
//        for (const auto &p: brain.neurons())
//          if (p->isHidden())
//            nlog << " " << p->value;
//        nlog << "\n";
//      }

//      if (olog.is_open()) {
//        for (const auto &v: s.neuralOutputs())
//          olog << v << " ";
//        olog << "\n";
//      }

//      if (mlog.is_open()) {
//        mlog << tags;
//        for (const auto &p: mann->modules()) {
//          if (p.second->type() == phenotype::ANN::Neuron::H) {
//            const auto &v = p.second->value();
//            mlog << " " << v.mean << " " << v.stddev;
//          }
//        }
//        mlog << "\n";
//      }
    }

    auto scores = scenario.scores();
    lhs.fitness = scores[0];
    rhs.fitness = scores[1];
    assert(!brainless[0] && !brainless[1]);
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

} // end of namespace simu
