#include "critter.h"

using namespace genotype;

Spline::Spline (void)
  : data {
    M_PI/2.,
    0, 0,

    0, 0,
    1, 0,

    0, 0, 0
  } {}

Spline Spline::zero(void) {
  return Spline ();
}

#define GENOME Spline

using D = GENOME::Data;
DEFINE_GENOME_FIELD_WITH_BOUNDS(D, data, "",
  D {    0,
     -M_PI/2., 0,
         0, -1, 0, -1,
         0, 0, 0 },
//  S { 0, -M_PI, 1, 0, 1, 0, 1, 0, 0, 0 },
//  S { 0, -M_PI, 1, 0, 1, 0, 1, 0, 0, 0 },
  D { M_PI,
      M_PI/2., 1,
      1, 1, 1, 1,
      .5, .5, .5 }
)

DEFINE_GENOME_MUTATION_RATES({
  EDNA_PAIR(data, 1),
})
DEFINE_GENOME_DISTANCE_WEIGHTS({
  EDNA_PAIR(data, 1.f),
})

#undef GENOME

#define GENOME Critter

DEFINE_GENOME_FIELD_AS_SUBGENOME(BOCData, cdata, "")

using Config = genotype::Critter::config_t;

using Ss = GENOME::Splines;
using S = Ss::value_type;

auto splinesFunctor = [] {
  GENOME_FIELD_FUNCTOR(Ss, splines) functor;

  functor.random = [] (auto&) {
    return Ss {};
  };

  functor.mutate = [] (auto &set, auto &dice) {
    auto &s = *dice(set.begin(), set.end());
    for (uint i=0; i<10; i++) s.mutate(dice);
  };

  functor.cross = [] (auto &lhs, auto &rhs, auto &dice) {
    Ss res;
    for (uint i=0; i<GENOME::SPLINES_COUNT; i++)
      res[i] = S::cross(lhs[i], rhs[i], dice);
    return res;
  };

  functor.distance = [] (auto &lhs, auto &rhs) {
    float d = 0;
    for (uint i=0; i<GENOME::SPLINES_COUNT; i++)
      d += S::distance(lhs[i], rhs[i]);
    return d;
  };

  functor.check = [] (auto &set) {
    bool ok = true;
    for (auto &s: set)  ok &= S::check(s);
    return ok;
  };

  return functor;
};
DEFINE_GENOME_FIELD_WITH_FUNCTOR(Ss, splines, "", splinesFunctor())

using Dm = GENOME::Dimorphism;
auto dimorphismFunctor = [] {
  GENOME_FIELD_FUNCTOR(Dm, dimorphism) functor;
  static constexpr auto C = GENOME::SPLINES_COUNT;
  using MO = config::MutationSettings::BoundsOperators<Dm::value_type>;

  functor.random = [] (auto&) {
    return utils::uniformStdArray<Dm>(0);
  };

  functor.mutate = [] (auto &set, auto &dice) {
    auto dmRates = Config::dimorphism_mutationRates();
    std::string field = dice.pickOne(dmRates);
    uint i = dice(uint(0), 2*C-1);
    if (field == "zero")        set[i] = 0;
    else if (field == "one")    set[i] = 1;
    else if (field == "equal")  set[i] = set[(i+C)%(2*C)];
    else if (field == "mut")    MO::mutate(set[i], 0, 1, dice);
  };

  functor.cross = [] (auto &lhs, auto &rhs, auto &dice) {
    Dm res;
    for (uint i=0; i<2*GENOME::SPLINES_COUNT; i++)
      res[i] = dice.toss(lhs[i], rhs[i]);
    return res;
  };

  functor.distance = [] (auto &lhs, auto &rhs) {
    float d = 0;
    for (uint i=0; i<GENOME::SPLINES_COUNT; i++)
      d += MO::distance(lhs[i], rhs[i], 0, 1);
    return d;
  };

  functor.check = [] (auto &set) {
    bool ok = true;
    for (auto &s: set)  ok &= MO::check(s, 0, 1);
    return ok;
  };

  return functor;
};
DEFINE_GENOME_FIELD_WITH_FUNCTOR(Dm, dimorphism, "", dimorphismFunctor())

DEFINE_GENOME_MUTATION_RATES({
  EDNA_PAIR(   splines, std::tuple_size_v<Ss> * std::tuple_size_v<D>),
  EDNA_PAIR(dimorphism, std::tuple_size_v<Dm> + 1),

  EDNA_PAIR(     cdata, 3.f),
})
DEFINE_GENOME_DISTANCE_WEIGHTS({
  EDNA_PAIR(   splines, std::tuple_size_v<Ss> * std::tuple_size_v<D>),
  EDNA_PAIR(dimorphism, std::tuple_size_v<Dm> + 1),

  EDNA_PAIR(     cdata, 0.f),
})

template <>
struct genotype::MutationRatesPrinter<Ss, GENOME, &GENOME::splines> {
  static constexpr bool recursive = false;
  static void print (std::ostream &os, uint width, uint depth, float ratio) {
    prettyPrintMutationRate(os, width, depth, ratio, 1, "mut", true);
  }
};

template <>
struct genotype::Extractor<Ss> {
  std::string operator() (const Ss &ls, const std::string &field) {
//    if (field.size() != 1 || !LSystem<L>::Rule::isValidNonTerminal(field[0]))
//      utils::doThrow<std::invalid_argument>(
//        "'", field, "' is not a valid non terminal");

//    auto it = ls.rules.find(field[0]);
//    if (it == ls.rules.end())
//      return "";
//    else
//      return it->second.rhs;
    return "Not implemented";
  }
};

template <>
struct genotype::Aggregator<Ss, Critter> {
  void operator() (std::ostream &os, const std::vector<Critter> &genomes,
                   std::function<const Ss& (const Critter&)> access,
                   uint /*verbosity*/) {
//    std::set<grammar::NonTerminal> nonTerminals;
//    for (const Plant &p: genomes)
//      for (const auto &pair: access(p).rules)
//        nonTerminals.insert(pair.first);

//    os << "\n";
//    utils::IndentingOStreambuf indent(os);
//    for (grammar::NonTerminal s: nonTerminals) {
//      std::map<grammar::Successor, uint> counts;
//      for (const Plant &p: genomes) {
//        const auto &map = access(p).rules;
//        auto it = map.find(s);
//        if (it != map.end())
//          counts[it->second.rhs]++;
//      }

//      uint i=0;
//      for (const auto &p: counts) {
//        if (i++ == 0)
//          os << s << " -> ";
//        else
//          os << "     ";
//        os << "(" << p.second << ") " << p.first << "\n";
//      }
//    }
  }
};

template <>
struct genotype::MutationRatesPrinter<Dm, GENOME, &GENOME::dimorphism> {
  static constexpr bool recursive = true;
  static void print (std::ostream &os, uint width, uint depth, float ratio) {
    const auto &r = Config::dimorphism_mutationRates();

#define F(X) r.at(X), X
    prettyPrintMutationRate(os, width, depth, ratio, F("zero"), false);
    prettyPrintMutationRate(os, width, depth, ratio, F("one"), false);
    prettyPrintMutationRate(os, width, depth, ratio, F("equal"), false);
    prettyPrintMutationRate(os, width, depth, ratio, F("mut"), false);
#undef F
  }
};

template <>
struct genotype::Extractor<Dm> {
  std::string operator() (const Dm &ls, const std::string &field) {
    return "Not implemented";
  }
};

template <>
struct genotype::Aggregator<Dm, Critter> {
  void operator() (std::ostream &os, const std::vector<Critter> &genomes,
                   std::function<const Dm& (const Critter&)> access,
                   uint /*verbosity*/) {}
};

#undef GENOME

#define CFILE genotype::Critter::config_t

DEFINE_SUBCONFIG(Config::Crossover, configCrossover)

DEFINE_CONTAINER_PARAMETER(CFILE::MutationRates, dimorphism_mutationRates,
                           utils::normalizeRates({
  {  "zero",  .5f },
  {   "one",  .5f },
  { "equal", 2.f  },
  {   "mut", 7.f  },
}))

#undef CFILE

