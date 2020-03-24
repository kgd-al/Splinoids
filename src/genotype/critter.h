#ifndef GNTP_CRITTER_H
#define GNTP_CRITTER_H

#include "kgd/genotype/selfawaregenome.hpp"

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

  static Spline zero (void);
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
  static constexpr uint SPLINES_COUNT = 4;

  using Sex = BOCData::Sex;

  using Splines = std::array<Spline, SPLINES_COUNT>;
  Splines splines;

  using Dimorphism = std::array<float, 2*SPLINES_COUNT>;
  Dimorphism dimorphism;

  using Colors = std::array<Color, 2*(SPLINES_COUNT+1)>;
  Colors colors;

  Vision vision;

  float minClockSpeed, maxClockSpeed;
  float matureAge, oldAge;

  BOCData cdata;
  phylogeny::Genealogy gdata;

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

  auto compatibility (float dist) const {
    return cdata(dist);
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

//  Critter();
};

DECLARE_GENOME_FIELD(Critter, Critter::Splines, splines)
DECLARE_GENOME_FIELD(Critter, Critter::Dimorphism, dimorphism)
DECLARE_GENOME_FIELD(Critter, Critter::Colors, colors)
DECLARE_GENOME_FIELD(Critter, Vision, vision)
DECLARE_GENOME_FIELD(Critter, BOCData, cdata)
DECLARE_GENOME_FIELD(Critter, float, minClockSpeed)
DECLARE_GENOME_FIELD(Critter, float, maxClockSpeed)
DECLARE_GENOME_FIELD(Critter, float, matureAge)
DECLARE_GENOME_FIELD(Critter, float, oldAge)

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

  using BC = Bounds<genotype::Color::value_type>;
  DECLARE_PARAMETER(BC, color_bounds)
  DECLARE_PARAMETER(MutationRates, colors_mutationRates)

  DECLARE_PARAMETER(Bounds<float>, minClockSpeedBounds)
  DECLARE_PARAMETER(Bounds<float>, maxClockSpeedBounds)
  DECLARE_PARAMETER(Bounds<float>, matureAgeBounds)
  DECLARE_PARAMETER(Bounds<float>, oldAgeBounds)

  using Crossover = genotype::BOCData::config_t;
  DECLARE_SUBCONFIG(Crossover, configCrossover)

  DECLARE_PARAMETER(MutationRates, mutationRates)
  DECLARE_PARAMETER(DistanceWeights, distanceWeights)
};

} // end of namespace config

#endif // GNTP_CRITTER_H
