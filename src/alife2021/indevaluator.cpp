#include "indevaluator.h"

namespace simu {

std::atomic<bool> IndEvaluator::aborted = false;

IndEvaluator::IndEvaluator (const genotype::Environment &e) : egenome(e) {
  static constexpr auto pos = {-1,+1};
  for (int fx: pos)
    for (int fy: pos)
       allScenarios[0].push_back(Specs::fromValues(Specs::ALONE, fx, fy));

  for (Specs::Type t: {Specs::CLONE, Specs::PREDATOR})
    for (int fx: pos)
      for (int fy: pos)
        for (int ox: pos)
          for (int oy: pos)
            allScenarios[int(t)].push_back(
              Specs::fromValues(t, fx, fy, ox, oy));
}


void IndEvaluator::selectCurrentScenarios (rng::AbstractDice &dice) {
  std::cout << "Scenarios for current generation: ";
  float diff = 0;
  for (uint i=0; i<S; i++) {
    const auto &s = currentScenarios[i] = *dice(allScenarios[i]);
    std::cout << " " << Specs::toString(s);
    diff += Specs::difficulty(s);
  }
  std::cout << "\nDifficulty: " << diff << "\n";
}

void IndEvaluator::setScenarios(const std::string &s) {
  for (const std::string str: utils::split(s, ';')) {
    auto specs = Specs::fromString(str);
    currentScenarios[specs.type] = specs;
  }
}

void IndEvaluator::operator() (Ind &ind, int) {
  float totalScore = 0;

  std::vector<phenotype::ANN> brains;

  bool brainless = false;
  for (uint i=0; i<S; i++) {
    Simulation simulation;
    Scenario scenario (currentScenarios[i], simulation);

    simulation.init(egenome, {}, simu::Scenario::commonInitData);
    scenario.init(ind.dna);

    brains.push_back(scenario.subjectBrain());
    brainless = scenario.subjectBrain().empty();
    if (brainless)  break;

    while (!simulation.finished() && !aborted)  simulation.step();

    float score = scenario.score();
    totalScore += score;
    ind.stats[scenarioLabels[i]] = score;
    assert(!brainless);
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
  for (uint i=0; i<S; i++)
    assert(!brainless || ind.stats[scenarioLabels[i]] == 0);
#endif

  ind.stats["b"] = !brainless;
  if (brainless)
   for (uint i=0; i<S; i++)
     ind.stats[scenarioLabels[i]] = 0;

  ind.fitnesses["fitness"] = totalScore;
}

} // end of namespace simu
