#ifndef VISU_CONFIG_H
#define VISU_CONFIG_H

#include "../simu/config.h"

#include <QPointF>

DEFINE_PRETTY_ENUMERATION(
  RenderingType,
  NORMAL, VISION)

QPointF toQt (const simu::P2D &p);

namespace config {

struct CONFIG_FILE(Visualisation) {
  DECLARE_SUBCONFIG(Simulation, configSimulation)

  DECLARE_PARAMETER(float, viewZoom)
  DECLARE_PARAMETER(bool, opaqueBodies)
  DECLARE_PARAMETER(bool, drawInnerEdges)

  DECLARE_PARAMETER(RenderingType, renderingType)

  DECLARE_PARAMETER(float, zoomFactor)
  DECLARE_PARAMETER(uint, substepsSpeed)

  using DebugDrawFlags = std::bitset<2*genotype::Critter::SPLINES_COUNT>;
#ifndef NDEBUG
  static DebugDrawFlags debugDraw;
  DECLARE_DEBUG_PARAMETER(int, showCollisionObjects, 0)
#else
  static constexpr DebugDrawFlags debugDraw = 0;
#endif
  DECLARE_DEBUG_PARAMETER(bool, b2DebugDraw, false)
};

} // end of namespace config

#endif // VISU_CONFIG_H
