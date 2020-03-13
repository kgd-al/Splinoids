#ifndef GNTP_CRITTER_H
#define GNTP_CRITTER_H

#include "kgd/genotype/selfawaregenome.hpp"

#include "kgd/apt/core/crossover.h"

namespace genotype {

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

class Critter : public genotype::EDNA<Critter> {
  APT_EDNA()

public:
  static constexpr uint SPLINES_COUNT = 4;

  using Sex = BOCData::Sex;

  using Splines = std::array<Spline, SPLINES_COUNT>;
  Splines splines;

  using Dimorphism = std::array<float, 2*SPLINES_COUNT>;
  Dimorphism dimorphism;

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
    return ".crt.json";
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
DECLARE_GENOME_FIELD(Critter, BOCData, cdata)

} // end of namespace gntp

namespace config {

template <>
struct EDNA_CONFIG_FILE(Spline) {
  DECLARE_PARAMETER(Bounds<genotype::Spline::Data>, dataBounds)

  DECLARE_PARAMETER(MutationRates, mutationRates)
  DECLARE_PARAMETER(DistanceWeights, distanceWeights)
};

template <>
struct EDNA_CONFIG_FILE(Critter) {
  DECLARE_PARAMETER(MutationRates, dimorphism_mutationRates)

  using Crossover = genotype::BOCData::config_t;
  DECLARE_SUBCONFIG(Crossover, configCrossover)

  DECLARE_PARAMETER(MutationRates, mutationRates)
  DECLARE_PARAMETER(DistanceWeights, distanceWeights)
};

} // end of namespace config

#endif // GNTP_CRITTER_H
