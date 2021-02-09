#ifndef VISU_ENVIRONMENT_H
#define VISU_ENVIRONMENT_H

#include <QGraphicsItem>

#include "../simu/environment.h"

namespace visu {

struct Critter;
struct Foodlet;

struct Overlay;
struct DebugDrawer;

class Environment : public QGraphicsItem {
  simu::Environment &_environment;

  using stvCritterMapping =
    std::function<const visu::Critter*(const simu::Critter*)>;
  const stvCritterMapping stvCritter;

  using stvFoodletMapping =
    std::function<const visu::Foodlet*(const simu::Foodlet*)>;
  const stvFoodletMapping stvFoodlet;

  Overlay *_overlay;
  friend struct Overlay;

  QPainterPath _edges;

#ifndef NDEBUG
  DebugDrawer *_ddrawer;
#endif

public:
  Environment (simu::Environment &e,
               stvCritterMapping stvC,
               stvFoodletMapping stvF);

  virtual ~Environment (void);

  Overlay* overlay (void) const {
    return _overlay;
  }

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

class Overlay : public QGraphicsItem { // Clone of env but at higher z value
  Environment &env;

public:
  Overlay (Environment &e, int zvalue);

  QRectF boundingRect(void) const {
    return env.boundingRect();
  }

  void paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*);

private:
  void drawFights (QPainter *painter, const QRectF &drawBounds) const;
};

} // end namespace visu

#endif // VISU_ENVIRONMENT_H
