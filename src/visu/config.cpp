#include "config.h"

#include "../simu/config.h"

namespace config {
#define CFILE Visualisation

DEFINE_SUBCONFIG(Simulation, configSimulation)

DEFINE_PARAMETER(float, viewZoom, 100)//100)
DEFINE_PARAMETER(bool, opaqueBodies, true)
DEFINE_PARAMETER(bool, drawInnerEdges, false)

DEFINE_PARAMETER(RenderingType, renderingType, RenderingType::NORMAL)

#ifndef NDEBUG
DEFINE_DEBUG_PARAMETER(int, showCollisionObjects, 0)

CFILE::DebugDrawFlags CFILE::debugDraw = 0;
#endif

#undef CFILE
} // end of namespace config
