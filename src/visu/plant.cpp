#include "plant.h"

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

Plant::Plant(simu::Plant &plant) : _plant(plant) {
//  setFlag(ItemIsSelectable, true);
  setAcceptedMouseButtons(Qt::NoButton);
  setPos(toQt(plant.body().GetPosition()));
  setZValue(1);
}

QRectF Plant::boundingRect (void) const {
  float S = _plant.size();
  return QRectF(-.5*S, -.5*S, S, S);
}

void Plant::paint(QPainter *painter,
                  const QStyleOptionGraphicsItem*, QWidget*) {
  float S = _plant.size();
  painter->setPen(Qt::NoPen);
  painter->setBrush(toQt(_plant.color()));
  painter->drawEllipse({0,0}, .5*S, .5*S);
}

} // end of namespace visu
