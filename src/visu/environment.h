#ifndef VISU_ENVIRONMENT_H
#define VISU_ENVIRONMENT_H

#include <QGraphicsItem>

#include "../simu/environment.h"

namespace visu {

struct DebugDrawer;

class Environment : public QGraphicsItem {
  simu::Environment &_environment;

#ifndef NDEBUG
  DebugDrawer *_ddrawer;
#endif

public:
  Environment (simu::Environment &e);

  virtual ~Environment (void);

  QRectF simuBoundingRect (void) const {
    const auto S = _environment.size();
    const auto HS = .5 * S;
    return QRectF(-HS, -HS, S, S);
  }

  QRectF boundingRect(void) const {
    const float PS = .5;
    QRectF r = simuBoundingRect();
    return r.adjusted(-PS, -PS, PS, PS);
  }

  void paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*);

#ifndef NDEBUG
  void doDebugDraw (void);

  DebugDrawer* debugDrawer (void) {
    return _ddrawer;
  }
#endif
};

} // end namespace visu

#endif // VISU_ENVIRONMENT_H
