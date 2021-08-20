#ifndef COEVOLUTIONGA_H
#define COEVOLUTIONGA_H

#include "indevaluator.h"

namespace ga {

class CoEvolution {
public:
  using Genome = simu::Evaluator::Genome;
  struct Parameters {
    uint teamSize, teams;
    uint generations;

    long seed;

    uint tournamentSize, elites;

    Parameters (void)
      : teamSize(1), teams(10), generations(1),
        seed(0),
        tournamentSize(3), elites(2) {}

    friend std::ostream& operator<< (std::ostream &os, const Parameters &p) {
      os << "===================\n"
            "== GA Parameters ==\n"
            "===================\n"
         << "   teamSize: " << p.teamSize << "\n"
         << "      teams: " << p.teams << "\n"
         << "generations: " << p.generations << "\n"
         << "       seed: " << p.seed << "\n"
         << " tournamentSize: " << p.seed << "\n"
         << "       seed: " << p.seed << "\n"
         << "===================\n";
      return os;
    }
  };

  CoEvolution (const Parameters &params);

private:
  const Parameters _params;
};

} // end of namespace ga

#endif // COEVOLUTIONGA_H
