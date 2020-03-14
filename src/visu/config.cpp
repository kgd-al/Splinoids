#include "config.h"

#include "../simu/config.h"

QPointF toQt (const simu::P2D &p) {
  return QPointF(p.x, p.y);
}

namespace config {
#define CFILE Visualisation

DEFINE_SUBCONFIG(Simulation, configSimulation)

DEFINE_PARAMETER(float, viewZoom, 100)//100)
DEFINE_PARAMETER(bool, opaqueBodies, true)
DEFINE_PARAMETER(bool, drawInnerEdges, false)

DEFINE_PARAMETER(RenderingType, renderingType, RenderingType::NORMAL)

DEFINE_PARAMETER(float, zoomFactor, 4)
DEFINE_PARAMETER(uint, substepsSpeed, 1)

#ifndef NDEBUG
DEFINE_DEBUG_PARAMETER(int, showCollisionObjects, 0)

CFILE::DebugDrawFlags CFILE::debugDraw = 0;
#endif
DEFINE_DEBUG_PARAMETER(bool, b2DebugDraw, false)

#undef CFILE
} // end of namespace config
