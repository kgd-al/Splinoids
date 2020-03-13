#ifndef VISU_ENVIRONMENT_H
#define VISU_ENVIRONMENT_H

#include <QGraphicsItem>

#include "../simu/environment.h"

namespace visu {

class Environment : public QGraphicsItem {
  const simu::Environment &_environment;

public:
  Environment (const simu::Environment &e);

  QRectF boundingRect (void) const override {
    const auto S = _environment.size();
    const auto HS = .5 * S;
    return QRectF(-HS, -HS, S, S);
  }

  void paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*);
};

} // end namespace visu

#endif // VISU_ENVIRONMENT_H
