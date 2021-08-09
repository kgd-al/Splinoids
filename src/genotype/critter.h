#ifndef GNTP_CRITTER_H
#define GNTP_CRITTER_H

#include "kgd/eshn/phenotype/ann.h"
#include "kgd/apt/core/crossover.h"

#include "config.h"

namespace genotype {

using Color = config::Color;

class Spline : public genotype::EDNA<Spline> {
  APT_EDNA()

public:
  using Data = std::array<float, 10>;
  enum Index {
    // Position of start point on body surface
    SA,

    // Position of end point relative to start
    EA, EL,

    // Control points (in local start-end coordinates)
    DX0, DY0, DX1, DY1,

    // Width variation (start, control 0 & 1)
    W0, W1, W2
  };

  Data data;

  Spline (void);
  Spline (const Data &data) : data(data) {}
};

DECLARE_GENOME_FIELD(Spline, Spline::Data, data)

class Vision : public genotype::EDNA<Vision> {
  APT_EDNA()
public:
  float angleBody, angleRelative;
  float width;

  uint precision;
};

DECLARE_GENOME_FIELD(Vision, float, angleBody)
DECLARE_GENOME_FIELD(Vision, float, angleRelative)
DECLARE_GENOME_FIELD(Vision, float, width)
DECLARE_GENOME_FIELD(Vision, uint, precision)

class Critter : public genotype::EDNA<Critter> {
  APT_EDNA()

public:
  #define CRITTER_SPLINES_COUNT 2
  static constexpr uint SPLINES_COUNT = CRITTER_SPLINES_COUNT;

  using Sex = BOCData::Sex;

  using Splines = std::array<Spline, SPLINES_COUNT>;
  Splines splines;

#ifdef USE_DIMORPHISM
  using Dimorphism = std::array<float, 2*SPLINES_COUNT>;
  Dimorphism dimorphism;

  using Colors = std::array<Color, 2*(SPLINES_COUNT+1)>;
#else
  using Colors = std::array<Color, SPLINES_COUNT+1>;
#endif
  Colors colors;

  Vision vision;

  float minClockSpeed, maxClockSpeed;
  float matureAge, oldAge;

  BOCData cdata;
  phylogeny::Genealogy gdata;

  enum ReproductionType { SEXUAL = 0, ASEXUAL = 1 };
  int asexual;

  ES_HyperNEAT brain;

  Critter (void) {}

  auto id (void) const {
    return gdata.self.gid;
  }

  auto species (void) const {
    return gdata.self.sid;
  }

  auto sex (void) const {
    return cdata.sex;
  }

  Critter clone (phylogeny::GIDManager &m) const {
    Critter other = *this;
    other.gdata.updateAfterCloning(m);
    return other;
  }

//  auto compatibility (float dist) const {
//    return cdata(dist);
//  }

  auto reproductiveInvestment (void) const {
    return .5;  // Definitely should depend on sex
  }

  static auto compatibility (const Critter &lhs, const Critter &rhs, double d) {
    return lhs.reproductiveInvestment() * lhs.cdata(d)
         + rhs.reproductiveInvestment() * rhs.cdata(d);
  }

  auto& genealogy (void) {
    return gdata;
  }

  const auto& genealogy (void) const {
    return gdata;
  }

  std::string extension (void) const override {
    return ".spln.json";
  }

  void to_jsonExtension (json &j) const override {
    j["gen"] = gdata;
  }

  void from_jsonExtension (json &j) override {
    auto it = j.find("gen");
    gdata = *it;
    j.erase(it);
  }

  void to_streamExtension (std::ostream &os) const override {
    os << gdata << "\n";
  }

  void equalExtension (const Critter &that, bool &ok) const override {
    ok &= (this->gdata == that.gdata);
  }

  // == Gaga required methods
  std::string serialize (void) const {
    return json(*this).dump();
  }

  Critter (const std::string &json) {
    *this = json::parse(json);
  }

  void reset (void) {}
};

DECLARE_GENOME_FIELD(Critter, Critter::Splines, splines)
#ifdef USE_DIMORPHISM
DECLARE_GENOME_FIELD(Critter, Critter::Dimorphism, dimorphism)
#endif
DECLARE_GENOME_FIELD(Critter, Critter::Colors, colors)
DECLARE_GENOME_FIELD(Critter, Vision, vision)
DECLARE_GENOME_FIELD(Critter, BOCData, cdata)
DECLARE_GENOME_FIELD(Critter, int, asexual)
DECLARE_GENOME_FIELD(Critter, float, minClockSpeed)
DECLARE_GENOME_FIELD(Critter, float, maxClockSpeed)
DECLARE_GENOME_FIELD(Critter, float, matureAge)
DECLARE_GENOME_FIELD(Critter, float, oldAge)
DECLARE_GENOME_FIELD(Critter, ES_HyperNEAT, brain)

} // end of namespace gntp

namespace config {

template <>
struct EDNA_CONFIG_FILE(Spline) {
  DECLARE_PARAMETER(Bounds<genotype::Spline::Data>, dataBounds)

  DECLARE_PARAMETER(MutationRates, mutationRates)
  DECLARE_PARAMETER(DistanceWeights, distanceWeights)
};

template <>
struct EDNA_CONFIG_FILE(Vision) {
  DECLARE_PARAMETER(Bounds<float>, angleBodyBounds)
  DECLARE_PARAMETER(Bounds<float>, angleRelativeBounds)
  DECLARE_PARAMETER(Bounds<float>, widthBounds)
  DECLARE_PARAMETER(Bounds<uint>, precisionBounds)

  DECLARE_PARAMETER(MutationRates, mutationRates)
  DECLARE_PARAMETER(DistanceWeights, distanceWeights)
};

template <>
struct EDNA_CONFIG_FILE(Critter) {
  DECLARE_PARAMETER(MutationRates, dimorphism_mutationRates)

//  using BC = Bounds<genotype::Color::value_type>;
//  DECLARE_PARAMETER(BC, color_bounds)
//  static constexpr float COLOR_MIN = .25f;
//  static constexpr float COLOR_MAX = .75f;

  using BC = Bounds<genotype::Color>;
  DECLARE_PARAMETER(BC, color_bounds)

  DECLARE_PARAMETER(MutationRates, colors_mutationRates)

  DECLARE_PARAMETER(Bounds<float>, minClockSpeedBounds)
  DECLARE_PARAMETER(Bounds<float>, maxClockSpeedBounds)
  DECLARE_PARAMETER(Bounds<float>, matureAgeBounds)
  DECLARE_PARAMETER(Bounds<float>, oldAgeBounds)
  DECLARE_PARAMETER(Bounds<uint>, brainSubstepsBounds)

  using Crossover = genotype::BOCData::config_t;
  DECLARE_SUBCONFIG(Crossover, configCrossover)
  DECLARE_SUBCONFIG(genotype::Spline::config_t, configSplines)
  DECLARE_SUBCONFIG(genotype::Vision::config_t, configVision)
  DECLARE_SUBCONFIG(config::EvolvableSubstrate, configBrain)

  DECLARE_PARAMETER(MutationRates, mutationRates)
  DECLARE_PARAMETER(DistanceWeights, distanceWeights)
};

} // end of namespace config

#endif // GNTP_CRITTER_H
