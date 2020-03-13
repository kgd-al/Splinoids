#ifndef VISU_CONFIG_H
#define VISU_CONFIG_H

#include "kgd/settings/configfile.h"

#include "../genotype/critter.h"

DEFINE_PRETTY_ENUMERATION(
  RenderingType,
  NORMAL, VISION)

namespace config {

struct Simulation;

struct CONFIG_FILE(Visualisation) {
  DECLARE_SUBCONFIG(Simulation, configSimulation)

  DECLARE_PARAMETER(float, viewZoom)
  DECLARE_PARAMETER(bool, opaqueBodies)
  DECLARE_PARAMETER(bool, drawInnerEdges)

  DECLARE_PARAMETER(RenderingType, renderingType)

  using DebugDrawFlags = std::bitset<2*genotype::Critter::SPLINES_COUNT>;
#ifndef NDEBUG
  static DebugDrawFlags debugDraw;
  DECLARE_DEBUG_PARAMETER(int, showCollisionObjects, 0)
#else
  static constexpr DebugDrawFlags debugDraw = 0;
#endif
};

} // end of namespace config

#endif // VISU_CONFIG_H
