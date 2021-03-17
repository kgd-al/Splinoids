#include "indevaluator.h"

namespace simu {

std::atomic<bool> IndEvaluator::aborted = false;

IndEvaluator::IndEvaluator (bool v0Scenarios): usingV0Scenarios(v0Scenarios) {
  static constexpr auto pos = {-1,+1};

  for (int fx: pos)
    for (int fy: pos)
       allScenarios[0].push_back(Specs::fromValues(Specs::V0_ALONE, fx, fy));

  for (Specs::Type t: {Specs::V0_CLONE, Specs::V0_PREDATOR})
    for (int fx: pos)
      for (int fy: pos)
        for (int ox: pos)
          for (int oy: pos)
            allScenarios[int(t)-1].push_back(
              Specs::fromValues(t, fx, fy, ox, oy));

  for (auto t=Specs::V1_ALONE; t<=Specs::V1_BOTH; t=Specs::Type(int(t)+1))
    for (int y: pos)
      allScenarios.back().push_back(Specs::fromValue(t, y));

  if (!v0Scenarios) setScenarios("all");

  lesions = {0};
}


void IndEvaluator::selectCurrentScenarios (rng::AbstractDice &dice) {
  if (!usingV0Scenarios) return;

  std::cout << "Scenarios for current generation: ";
  float diff = 0;
  currentScenarios.resize(3);
  for (uint i=0; i<S; i++) {
    const auto &s = currentScenarios[i] = *dice(allScenarios[i]);
    std::cout << " " << Specs::toString(s);
    diff += Specs::difficulty(s);
  }
  std::cout << "\nDifficulty: " << diff << "\n";
}

void IndEvaluator::setScenarios(const std::string &s) {
  if (s == "all") {
    if (usingV0Scenarios) {
      for (uint i=0; i<S; i++)
        for (const auto &s: allScenarios[i])
          currentScenarios.push_back(s);
    } else
      currentScenarios = allScenarios.back();
  } else {
    currentScenarios.clear();
    for (const std::string str: utils::split(s, ';')) {
      auto specs = Specs::fromString(str);
      currentScenarios.push_back(specs);
    }
  }
}

void IndEvaluator::setLesionTypes(const std::string &s) {
  if (s.empty())
    return;

  else if (s == "all")
    lesions = {0,1,2,3};

  else {
    for (const std::string str: utils::split(s, ';')) {
      int i;
      if (std::istringstream (str) >> i)
        lesions.push_back(i);
    }
  }
}

void IndEvaluator::applyNeuralFlags(phenotype::ANN &ann,
                                    const std::string &tagsfile) {
  auto &n = ann.neurons();

  std::ifstream ifs (tagsfile);
  if (!ifs)
    utils::doThrow<std::invalid_argument>(
      "Failed to open neural tags file '", tagsfile, "'");

  for (std::string line; std::getline(ifs, line); ) {
    if (line.empty() || line[0] == '/') continue;
    std::istringstream iss (line);
    phenotype::ANN::Point pos;
    iss >> pos;

    auto it = n.find(pos);
    if (it == n.end())
      utils::doThrow<std::invalid_argument>(
        "No neuron at position {", pos.x(), ", ", pos.y(), "}");

    iss >> it->second->flags;
  }

  if (config::Simulation::verbosity() > 0) {
    std::cout << "Neural flags:\n";
    for (const auto &p: n)
      std::cout << "\t" << std::setfill(' ') << std::setw(10) << p.first
                << ":\t"
                << std::bitset<8*sizeof(p.second->flags)>(p.second->flags)
                << "\n";
  }
}

void IndEvaluator::operator() (Ind &ind, int) {
  float totalScore = 0;

  const auto specToString = [this] (const Specs s, int lesion) {
    std::ostringstream oss;
    oss << Specs::toString(s);
    if (lesion > 0) oss << "_l" << lesion;
    return oss.str();
  };

#ifndef NDEBUG
  std::vector<phenotype::ANN> brains;
#endif

  ind.signature.fill(0);

  bool brainless = false;
  uint sid = 0;
  for (const Specs &spec: currentScenarios) {
    using utils::operator<<;
    for (int lesion: lesions) {
//      if (lesion != 0 && spec.type != Specs::V1_BOTH) continue;

      std::string specStr = specToString(spec, lesion);

      Simulation simulation;
      Scenario scenario (spec, simulation);

      scenario.init(ind.dna);
      const Critter &s = *scenario.subject();
      const auto &brain = s.brain();

  #ifndef NDEBUG
      if (lesion == 0)  brains.push_back(brain);
  #endif

      brainless = brain.empty();
      if (brainless)  break;

      std::unique_ptr<phenotype::ModularANN> mann;
      if (!logsSavePrefix.empty() && !annTagsFile.empty()) {
        applyNeuralFlags(scenario.subject()->brain(), annTagsFile);
        mann = std::make_unique<phenotype::ModularANN>(brain);
      }
      scenario.applyLesions(lesion);

      std::bitset<5> tags;
      std::ofstream tlog, alog, nlog;
      std::ofstream olog, mlog;
      if (!logsSavePrefix.empty()) {
        stdfs::path savePath = logsSavePrefix / specStr;

        stdfs::create_directories(savePath);

        std::string stags = "GAENV";

        tlog.open(savePath / "trajectory.dat");
        tlog << "Env size: " << simulation.environment().xextent()
             << " " << simulation.environment().yextent() << "\n"
             << "Food_x Food_y Food_r\n";
        if (auto f = scenario.foodlet())
          tlog << f->x() << " " << f->y() << " " << f->radius() << "\n\n";

        tlog << stags << " sx sy sa cx cy ca px py pa\n";

        alog.open(savePath / "vocalisation.dat");
        alog << "Noise";
        for (uint i=0; i<Critter::VOCAL_CHANNELS; i++)
          alog << " S" << i;
        for (uint i=0; i<Critter::VOCAL_CHANNELS; i++)
          alog << " C" << i;
        alog << "\n";

        nlog.open(savePath / "neurons.dat");
        nlog << stags;
        for (const auto &p: brain.neurons())
          if (p.second->isHidden())
            nlog << " (" << p.first.x() << "," << p.first.y() << ")";
        nlog << "\n";

        if (!annTagsFile.empty()) {
          olog.open(savePath / "outputs.dat");
          olog << "ML MR CS VV VC\n";

          mlog.open(savePath / "modules.dat");
          mlog << stags;
          for (const auto &p: mann->modules()) {
            if (p.second->type() == phenotype::ANN::Neuron::H) {
              auto f = p.second->flags;
              mlog << " " << f << "M " << f << "S";
            }
          }
          mlog << "\n";

          std::ofstream slog (savePath / "nstats");
          std::array<uint, 3> n {0};
          uint c = 0;
          float s = 0;
          for (const auto &p: brain.neurons()) {
            n[p.second->type]++;
            c += p.second->links().size();
            s += p.first.x();
          }
          slog << "INeurons: " << n[0] << "\n"
               << "ONeurons: " << n[1] << "\n"
               << "HNeurons: " << n[2] << "\n"
               << "Connections: " << c << "\n";
          slog << "Skewdness: " << s / brain.neurons().size() << "\n";
          n.fill(0);
          c = 0;
          for (const auto &p: mann->modules()) {
            n[p.second->type()]++;
            c += p.second->links.size();
          }
          slog << "IModules: " << n[0] << "\n"
               << "OModules: " << n[1] << "\n"
               << "HModules: " << n[2] << "\n"
               << "MConnections: " << c << "\n";
        }
      }

      while (!simulation.finished() && !aborted) {
        simulation.step();

        // Update modules values (if modular ann is used)
        if (mann) for (const auto &p: mann->modules()) p.second->update();

        if (!logsSavePrefix.empty()) {
          tags.reset();
          const auto &r = s.retina();
          tags[4] = std::any_of(r.begin(), r.end(),
            [] (const auto &c) { return c[0] == 0 && c[1] > 0 && c[2] == 0; });

          tags[3] = std::any_of(r.begin(), r.end(),
            [] (const auto &c) { return std::all_of(c.begin(), c.end(),
              [] (auto v) { return 0 < v && v < 1; }); });

          tags[2] = std::any_of(r.begin(), r.end(),
            [] (const auto &c) { return c[0] == 1; });

          const auto &e = s.ears();
          for (uint i=0; i<e.size(); i++) {
            uint ti = (i%(Critter::VOCAL_CHANNELS+1) == 0) ? 1 : 0;
            tags[ti] = tags[ti] | (e[i] > 0);
          }
        }

        if (tlog.is_open()) {
          tlog << tags
               << " " << s.x() << " " << s.y() << " " << s.rotation() << " ";

          if (auto c = scenario.clone())
            tlog << c->x() << " " << c->y() << " " << c->rotation();
          else
            tlog << "nan nan nan";
          tlog << " ";

          if (auto p = scenario.predator())
            tlog << p->x() << " " << p->y() << " " << p->rotation();
          else
            tlog << "nan nan nan";

          tlog << "\n";
        }

        if (alog.is_open()) {
          for (float v: s.producedSound())  alog << v << " ";

          if (auto c = scenario.clone())
            for (uint i=0; i<Critter::VOCAL_CHANNELS; i++)
              alog << c->producedSound()[i+1] << " ";
          else
            for (uint i=0; i<Critter::VOCAL_CHANNELS; i++)
              alog << "nan ";
          alog << "\n";
        }

        if (nlog.is_open()) {
          nlog << tags;
          for (const auto &p: brain.neurons())
            if (p.second->isHidden())
              nlog << " " << p.second->value;
          nlog << "\n";
        }

        if (olog.is_open()) {
          for (const auto &v: s.neuralOutputs())
            olog << v << " ";
          olog << "\n";
        }

        if (mlog.is_open()) {
          mlog << tags;
          for (const auto &p: mann->modules()) {
            if (p.second->type() == phenotype::ANN::Neuron::H) {
              const auto &v = p.second->value();
              mlog << " " << v.mean << " " << v.stddev;
            }
          }
          mlog << "\n";
        }
      }

      float score = scenario.score();
      if (lesion == 0)  totalScore += score;
      ind.stats[specStr] = score;
      if (lesions.empty() && !(scenario.specs().type & Specs::EVAL)) {
        ind.signature[2*sid] = scenario.subject()->x();
        ind.signature[2*sid+1] = scenario.subject()->y();
        sid++;
      }
      assert(!brainless);
    }
  }

#ifndef NDEBUG
  for (auto &b: brains) b.reset();
  for (uint i=0; i<brains.size(); i++) {
    for (uint j=i+1; j<brains.size(); j++) {
      static const auto print = [] (const auto &j, const std::string &l) {
        std::ofstream ("mismatched_ann_" + l) << j;
      };
      nlohmann::json jlhs = brains[i], jrhs = brains[j];
      if (jlhs != jrhs) {
        std::ofstream ("mismatched.spln.json") << nlohmann::json(ind.dna);
        print(jlhs, "lhs");
        print(jrhs, "rhs");
        brains[i].render_gvc_graph("mismatched_lhs.png");
        brains[j].render_gvc_graph("mismatched_rhs.png");
      }
      assertEqual(brains[i], brains[j], true);
    }
  }

  assert(!brainless || totalScore == 0);
  for (const Specs &spec: currentScenarios)
    assert(!brainless || ind.stats[Specs::toString(spec)] == 0);
#endif

  ind.stats["brain"] = !brainless;
  if (brainless)
   for (const Specs &spec: currentScenarios)
     ind.stats[Specs::toString(spec)] = 0;

  ind.fitnesses["fitness"] = totalScore;

  if (!logsSavePrefix.empty()) {
    std::ofstream ofs (logsSavePrefix / "scores.dat");
    for (auto &stat: ind.stats) {
      ofs << stat.first << " " << stat.second << "\n";
    }
  }
}

IndEvaluator::Ind IndEvaluator::fromJsonFile(const std::string &path) {
  std::ifstream t(path);
  std::stringstream buffer;
  buffer << t.rdbuf();
  auto o = nlohmann::json::parse(buffer.str());

  if (o.count("dna")) // assuming this is a gaga individual
    return Ind(o);
  else {
    DNA g = o;
    return Ind(g);
  }
}

} // end of namespace simu
