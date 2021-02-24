#ifndef VISU_OBSTACLE_H
#define VISU_OBSTACLE_H

#include <QGraphicsItem>

#include "box2d/b2_body.h"

namespace visu {

class Obstacle : public QGraphicsItem {
private:
  b2Body &_obstacle;
  QPainterPath _body;

public:
  Obstacle(b2Body &body);

  const auto& object (void) const {
    return _obstacle;
  }

  auto& object (void) {
    return _obstacle;
  }

  QRectF boundingRect (void) const;

  void paint(QPainter *painter,
             const QStyleOptionGraphicsItem*, QWidget *) override;
};

} // end of namespace visu

#endif // VISU_OBSTACLE_H
