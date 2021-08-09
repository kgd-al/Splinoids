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

using I = Spline::Index;
struct SplineMaker {
  Spline::Data d;
  SplineMaker (void) {
    d.fill(NAN);
  }
  SplineMaker& operator() (I i, float v) {
    d[i] = v;
    return *this;
  }
  auto operator() (void) {
    if (std::any_of(d.begin(), d.end(),
                    static_cast<bool(*)(float)>(std::isnan)))
      utils::doThrow<std::invalid_argument>(
        "Uninitialized field in spline: ", d, "!");
    return d;
  }
};


#define GENOME Spline

using D = GENOME::Data;
DEFINE_GENOME_FIELD_WITH_BOUNDS(D, data, "",
  SplineMaker()
    (I::SA, 0)
    (I::EA, -M_PI/2) (I::EL, 0)
    (I::DX0, 0)(I::DY0, -1)(I::DX1, 0)(I::DY1, -1)
    (I::W0, 0) (I::W1, 0) (I::W2, 0)
    (),

  SplineMaker()
    (I::SA, 0)
    (I::EA, 0) (I::EL, 0)
    (I::DX0, 0)(I::DY0, 0)(I::DX1, 0)(I::DY1, 0)
    (I::W0, 0) (I::W1, 0) (I::W2, 0)
    (),

  SplineMaker()
    (I::SA, M_PI)
    (I::EA, 0) (I::EL, 0)
    (I::DX0, 1)(I::DY0, 0)(I::DX1, 1)(I::DY1, 0)
    (I::W0, 0) (I::W1, 0) (I::W2, 0)
    (),

  SplineMaker()
    (I::SA, M_PI)
    (I::EA, M_PI/2) (I::EL, 1)
    (I::DX0, 1)(I::DY0, 1)(I::DX1, 1)(I::DY1, 1)
    (I::W0, .5) (I::W1, .5) (I::W2, .5)
    (),

  config::MutationSettings::stddev_t<D>::type {
      1e-2,
      1e-1, 1e-1,
      2e-1, 2e-1, 2e-1, 2e-1,
      1e-1, 1e-1, 1e-1
  }
)

DEFINE_GENOME_MUTATION_RATES({
  EDNA_PAIR(data, 1),
})
DEFINE_GENOME_DISTANCE_WEIGHTS({
  EDNA_PAIR(data, 1.f),
})

#undef GENOME


#define GENOME Vision

static constexpr float fPI = M_PI;
static constexpr float aVI = fPI / 4;
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, angleBody, "", 0.f, aVI, aVI, fPI/2.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, angleRelative, "", -fPI/2.f, 0.f, 0.f, fPI/2.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, width, "", fPI/60, fPI/2, fPI/2, 2*fPI/3.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(uint, precision, "", 0u, 1u, 1u, 5u)

DEFINE_GENOME_MUTATION_RATES({
  EDNA_PAIR(    angleBody, 1),
  EDNA_PAIR(angleRelative, 1),
  EDNA_PAIR(        width, 1),
  EDNA_PAIR(    precision, 1),
})
DEFINE_GENOME_DISTANCE_WEIGHTS({
  EDNA_PAIR(    angleBody, 1),
  EDNA_PAIR(angleRelative, 1),
  EDNA_PAIR(        width, 1),
  EDNA_PAIR(    precision, 1),
})

#undef GENOME


#define GENOME Critter
using Config = genotype::Critter::config_t;

DEFINE_GENOME_FIELD_AS_SUBGENOME(BOCData, cdata, "")
DEFINE_GENOME_FIELD_AS_SUBGENOME(Vision, vision, "")
DEFINE_GENOME_FIELD_AS_SUBGENOME(ES_HyperNEAT, brain, "")

DEFINE_GENOME_FIELD_WITH_BOUNDS(float, minClockSpeed, "minCS", .1f, 1.f, 1.f, 1.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, maxClockSpeed, "maxCS", 1.f, 1.f, 1.f, 2.f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, matureAge, "mature", .25f, .33f, .33f, .5f)
DEFINE_GENOME_FIELD_WITH_BOUNDS(float, oldAge, "old", .5f, .66f, .66f, .75f)

auto sexualityFunctor = [] {
  GENOME_FIELD_FUNCTOR(int, asexual) functor;

  functor.random = [] (auto &/*dice*/) {
    return Critter::ASEXUAL;
  };

  functor.mutate = [] (auto &/*a*/, auto &/*dice*/) { (void)log; };

  functor.cross = [] (auto &lhs, auto &rhs, auto &dice) {
    return dice.toss(lhs, rhs);
  };

  functor.distance = [] (auto &/*lhs*/, auto &/*rhs*/) {
    float d = 0;
    return d;
  };

  functor.check = [] (auto &/*set*/) {
    bool ok = true;
    return ok;
  };

  return functor;
};
DEFINE_GENOME_FIELD_WITH_FUNCTOR(int, asexual, "", sexualityFunctor())

using Ss = GENOME::Splines;
using S = Ss::value_type;

auto splinesFunctor = [] {
  GENOME_FIELD_FUNCTOR(Ss, splines) functor;

  functor.random = [] (auto &dice) {
    Ss set;
    for (S &s: set) s = S::random(dice);
    return set;
  };

  functor.mutate = [] (auto &set, auto &dice) {
    auto it = dice(set.begin(), set.end());
    if (log())
      std::cerr << " at index " << std::distance(set.begin(), it) << "\n\t";
    it->mutate(dice);
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

#ifdef USE_DIMORPHISM
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
    if (log()) std::cerr << " no log for dimorphism mutations";
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
#endif

using Cs = GENOME::Colors;
auto colorsFunctor = [] {
  GENOME_FIELD_FUNCTOR(Cs, colors) functor;
  static constexpr auto Csn = GENOME::SPLINES_COUNT+1;  // Also body color
  static constexpr auto Cn = std::tuple_size_v<Color>;

  using MO = config::MutationSettings::BoundsOperators<Color::value_type>;

  functor.random = [] (auto &dice) {
    static const Config::BC &bounds = Config::color_bounds();
    Cs colors;
    for (Color &c: colors)
      for (uint j=0; j<Cn; j++)
        c[j] = dice(bounds.rndMin[j], bounds.rndMax[j]);
    return colors;
  };

  functor.mutate = [] (auto &colors, auto &dice) {
    static const Config::BC &bounds = Config::color_bounds();
    static const auto &dmRates = Config::colors_mutationRates();
    std::string field = dice.pickOne(dmRates);
    uint i = dice(Cs::size_type(0), colors.size()-1);
    if (field == "monomorphism") {
      colors[i] = colors[(i+Csn)%(2*Csn)];
      if (log()) std::cerr << " sexual monomorphism at index " << i;

    } else if (field == "homogeneous") {
      uint j = i;
      while (j == i) {
        j = dice(Csn*floor(j/Csn), Csn*(1+floor(j/Csn))-1);
      }
      colors[i] = colors[j];
      if (log()) std::cerr << " color[" << i << "] <- color[" << j << "]";

    } else if (field == "mutate") {
      /// TODO Deactivated red component mutation
      uint j = dice(Color::size_type(1), Cn-1);
      if (log())
        std::cerr << " mutating color[" << i << "][" << j << "] from "
                  << colors[i][j];
      MO::mutate(colors[i][j], bounds.min[j], bounds.max[j], bounds.stddev[j],
                 dice);
      if (log())  std::cerr << " to " << colors[i][j];
    }
  };

  functor.cross = [] (auto &lhs, auto &rhs, auto &dice) {
    Cs res;
    for (uint i=0; i<2*Csn; i++)
      for (uint j=0; j<Cn; j++)
        res[i][j] = dice.toss(lhs[i][j], rhs[i][j]);
    return res;
  };

  functor.distance = [] (auto &lhs, auto &rhs) {
    static const Config::BC &bounds = Config::color_bounds();
      float d = 0;
    for (uint i=0; i<2*Csn; i++)
      for (uint j=0; j<Cn; j++)
        d += MO::distance(lhs[i][j], rhs[i][j], bounds.min[j], bounds.max[j]);
    return d;
  };

  functor.check = [] (auto &colors) {
    static const Config::BC &bounds = Config::color_bounds();
    bool ok = true;
    for (auto &c: colors)
      for (uint j=0; j<Cn; j++)
        ok &= MO::check(c[j], bounds.min[j], bounds.max[j]);
    return ok;
  };

  return functor;
};
DEFINE_GENOME_FIELD_WITH_FUNCTOR(Cs, colors, "", colorsFunctor())

#ifdef USE_DIMORPHISM
#define DIMORPHISM_MR EDNA_PAIR(   dimorphism, 1.5 * std::tuple_size_v<Dm>),
#define DIMORPHISM_DW EDNA_PAIR(   dimorphism, std::tuple_size_v<Dm> + 1),
#else
#define DIMORPHISM_MR
#define DIMORPHISM_DW
#endif

DEFINE_GENOME_MUTATION_RATES({
  EDNA_PAIR(      splines, std::tuple_size_v<Ss> * std::tuple_size_v<D>),

  DIMORPHISM_MR

  EDNA_PAIR(       colors, .5*std::tuple_size_v<Cs>),
  EDNA_PAIR(       vision, 4),
  EDNA_PAIR(minClockSpeed, 1),
  EDNA_PAIR(maxClockSpeed, 1),
  EDNA_PAIR(    matureAge, 1),
  EDNA_PAIR(       oldAge, 1),
  EDNA_PAIR(      asexual, 0),

  EDNA_PAIR(        brain, 40),

  EDNA_PAIR(        cdata, 3.f),
})
DEFINE_GENOME_DISTANCE_WEIGHTS({
  EDNA_PAIR(      splines, std::tuple_size_v<Ss> * std::tuple_size_v<D>),

  DIMORPHISM_DW

  EDNA_PAIR(       colors, std::tuple_size_v<Cs>),
  EDNA_PAIR(       vision, 4),
  EDNA_PAIR(minClockSpeed, 1),
  EDNA_PAIR(maxClockSpeed, 1),
  EDNA_PAIR(    matureAge, 1),
  EDNA_PAIR(       oldAge, 1),
  EDNA_PAIR(      asexual, 0),

  EDNA_PAIR(        brain, 9),

  EDNA_PAIR(        cdata, 0.f),
})

template <>
struct genotype::MutationRatesPrinter<int, GENOME, &GENOME::asexual> {
  static constexpr bool recursive = false;
  static void print (std::ostream &os, uint width, uint depth, float ratio) {
    prettyPrintMutationRate(os, width, depth, ratio, 1, "mut", true);
  }
};

template <>
struct genotype::MutationRatesPrinter<Ss, GENOME, &GENOME::splines> {
  static constexpr bool recursive = false;
  static void print (std::ostream &os, uint width, uint depth, float ratio) {
    prettyPrintMutationRate(os, width, depth, ratio, 1, "mut", true);
  }
};

template <>
struct genotype::Extractor<Ss> {
  std::string operator() (const Ss &/*ls*/, const std::string &/*field*/) {
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

#ifdef USE_DIMORPHISM
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
  std::string operator() (const Dm &/*ls*/, const std::string &/*field*/) {
    return "Not implemented";
  }
};
#endif

template <>
struct genotype::MutationRatesPrinter<Cs, GENOME, &GENOME::colors> {
  static constexpr bool recursive = true;
  static void print (std::ostream &os, uint width, uint depth, float ratio) {
    const auto &r = Config::colors_mutationRates();

#define F(X) r.at(X), X
#ifdef USE_DIMORPHISM
    prettyPrintMutationRate(os, width, depth, ratio, F("monomorphism"), false);
#endif
    prettyPrintMutationRate(os, width, depth, ratio, F("homogeneous"), false);
    prettyPrintMutationRate(os, width, depth, ratio, F("mutate"), false);
#undef F
  }
};

template <>
struct genotype::Extractor<Cs> {
  std::string operator() (const Cs &/*ls*/, const std::string &/*field*/) {
    return "Not implemented";
  }
};

#undef GENOME

#define CFILE genotype::Critter::config_t

DEFINE_SUBCONFIG(genotype::Spline::config_t, configSplines)
DEFINE_SUBCONFIG(genotype::Vision::config_t, configVision)
DEFINE_SUBCONFIG(Config::Crossover, configCrossover)
DEFINE_SUBCONFIG(config::EvolvableSubstrate, configBrain)

#ifdef USE_DIMORPHISM
DEFINE_CONTAINER_PARAMETER(CFILE::MutationRates, dimorphism_mutationRates,
                           utils::normalizeRates({
  {  "zero",  .5f },
  {   "one",  .5f },
  { "equal", 2.f  },
  {   "mut", 7.f  },
}))

#define COLOR_DIMORPHISM_MUTATION_RATES { "monomorphism", 9.f  },
#else
#define COLOR_DIMORPHISM_MUTATION_RATES
#endif


DEFINE_CONTAINER_PARAMETER(CFILE::MutationRates, colors_mutationRates,
                           utils::normalizeRates({
  {  "homogeneous", Critter::SPLINES_COUNT > 0 ? 1 : 0  },
  COLOR_DIMORPHISM_MUTATION_RATES
  {       "mutate", 490.f  },
}))

genotype::Color color (float g, float b) {
  return {0,g,b};
}
DEFINE_PARAMETER(CFILE::BC, color_bounds,
                 color(.1,.1),    color(.25,.25),
                 color(.75,.75),  color(1,1))

#undef CFILE
