#ifndef VISU_CONFIG_H
#define VISU_CONFIG_H

#include "../simu/config.h"

#include <QPointF>
#include <QColor>

QPointF toQt (const simu::P2D &p);
QColor toQt (const simu::Color &c);

DEFINE_PRETTY_ENUMERATION(RenderingType, REGULAR, ENERGY, HEALTH)

namespace config {

struct CONFIG_FILE(Visualisation) {
  DECLARE_SUBCONFIG(Simulation, configSimulation)

  DECLARE_PARAMETER(float, viewZoom)
  DECLARE_PARAMETER(bool, opaqueBodies)
  DECLARE_PARAMETER(bool, drawInnerEdges)
  DECLARE_PARAMETER(int, drawVision)

  DECLARE_PARAMETER(RenderingType, renderType)

  DECLARE_PARAMETER(bool, brainDeadSelection)

  DECLARE_PARAMETER(int, showFights)

  DECLARE_PARAMETER(bool, showGrid)

  DECLARE_PARAMETER(float, selectionZoomFactor)
  DECLARE_PARAMETER(uint, substepsSpeed)

  DECLARE_DEBUG_PARAMETER(int, drawFightingDebug, 0)

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
