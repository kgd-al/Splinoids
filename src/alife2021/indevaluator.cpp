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
            allScenarios[int(t)].push_back(
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

  bool brainless = false;
  for (const Specs &spec: currentScenarios) {
    using utils::operator<<;
    for (int lesion: lesions) {
      std::string specStr = specToString(spec, lesion);

      Simulation simulation;
      Scenario scenario (spec, simulation);

      scenario.init(ind.dna, lesion);
      const Critter &s = *scenario.subject();

  #ifndef NDEBUG
      brains.push_back(s.brain());
  #endif

      brainless = s.brain().empty();
      if (brainless)  break;

      std::bitset<3> tags;
      std::ofstream tlog, alog, nlog;
      if (!logsSavePrefix.empty()) {
        stdfs::path savePath = logsSavePrefix / specStr;

        stdfs::create_directories(savePath);

        std::string stags = "GAE";

        tlog.open(savePath / "trajectory.dat");
        tlog << "Env size: " << simulation.environment().extent() << "\n"
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
        for (const auto &p: s.brain().neurons())
          if (p.second->isHidden())
            nlog << " (" << p.first.x() << "," << p.first.y() << ")";
        nlog << "\n";
      }

      while (!simulation.finished() && !aborted) {
        simulation.step();

        if (!logsSavePrefix.empty()) {
          const auto r = s.retina();
          tags[2] = std::any_of(r.begin(), r.end(),
            [] (const auto &c) { return c[0] == 0 && c[1] > 0 && c[2] == 0; });

          tags[1] = std::any_of(r.begin(), r.end(),
            [] (const auto &c) { return std::all_of(c.begin(), c.end(),
              [] (auto v) { return 0 < v && v < 1; }); });

          tags[0] = std::any_of(r.begin(), r.end(),
            [] (const auto &c) { return c[0] == 1; });
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
          for (const auto &p: s.brain().neurons())
            if (p.second->isHidden())
              nlog << " " << p.second->value;
          nlog << "\n";
        }
      }

      float score = scenario.score();
      if (lesion == 0)  totalScore += score;
      ind.stats[specStr] = score;
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
