#include "foodlet.h"

#include "config.h"

#include <QVector2D>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QPainterPath>
#include <QtMath>
#include <QFileDialog>

#include "box2d/b2_fixture.h"
#include "box2d/b2_polygon_shape.h"

#include <QDebug>

namespace visu {

Foodlet::Foodlet(simu::Foodlet &foodlet) : _foodlet(foodlet) {
//  setFlag(ItemIsSelectable, true);
  setAcceptedMouseButtons(Qt::NoButton);
  setPos(toQt(_foodlet.body().GetPosition()));
  setZValue(1);
}

QRectF Foodlet::boundingRect (void) const {
  float R = _foodlet.radius();
  return QRectF(-R, -R, 2*R, 2*R);
}

void Foodlet::paint(QPainter *painter,
                  const QStyleOptionGraphicsItem*, QWidget*) {
  float R = _foodlet.radius();
  painter->setPen(Qt::NoPen);
  painter->setBrush(toQt(_foodlet.color()));
  painter->drawEllipse({0,0}, R, R);
}

} // end of namespace visu
