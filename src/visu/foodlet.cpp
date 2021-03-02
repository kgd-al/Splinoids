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

  if (!tag.isEmpty()) {
    painter->save();
    painter->scale(1, -1);

    painter->setPen(Qt::black);
    auto rect = boundingRect().translated(0, .4).adjusted(0, -.25, 0, 0);
    QFont f = painter->font();
    f.setPointSizeF(.1*f.pointSizeF());
    painter->setFont(f);

    painter->drawText(rect, Qt::AlignCenter, tag);
    painter->restore();
  }
}

} // end of namespace visu
