#include "config.h"

#include "../simu/config.h"

QPointF toQt (const simu::P2D &p) {
  return QPointF(p.x, p.y);
}

QColor toQt (const simu::Color &c) {
  return QColor::fromRgbF(c[0], c[1], c[2], 1);
}

namespace config {
#define CFILE Visualisation

DEFINE_SUBCONFIG(Simulation, configSimulation)

DEFINE_PARAMETER(float, viewZoom, 100)//100)
DEFINE_PARAMETER(bool, opaqueBodies, true)
DEFINE_PARAMETER(bool, drawInnerEdges, false)
DEFINE_PARAMETER(int, drawVision, 0)
DEFINE_PARAMETER(int, drawReproduction, 1)
DEFINE_PARAMETER(RenderingType, renderType, RenderingType::REGULAR)

DEFINE_PARAMETER(bool, brainDeadSelection, false)

DEFINE_PARAMETER(int, showFights, 2)

DEFINE_PARAMETER(bool, showGrid, true)

DEFINE_PARAMETER(float, selectionZoomFactor, 8)
DEFINE_PARAMETER(uint, substepsSpeed, 1)

DEFINE_DEBUG_PARAMETER(bool, ghostMode, false)
DEFINE_DEBUG_PARAMETER(int, drawFightingDebug, 5)

#ifndef NDEBUG
DEFINE_DEBUG_PARAMETER(int, showCollisionObjects, 0)

CFILE::DebugDrawFlags CFILE::debugDraw = 0;
#endif
DEFINE_DEBUG_PARAMETER(bool, b2DebugDraw, false)

#undef CFILE
} // end of namespace config
