#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <QDebug>

#include "environment.h"

#include "../simu/config.h"
#include "config.h"

namespace visu {

Environment::Environment (const simu::Environment &e) : _environment(e) {
  setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
  setZValue(-20);
}

void drawVisionUnderlay (QPainter *painter, const QRectF &r) {
  painter->save();
    QPen pen = painter->pen();
    pen.setWidthF(0);
    pen.setColor(Qt::lightGray);
    painter->setPen(pen);

    qDebug() << r;

    static constexpr auto S = config::Simulation::GRID_SUBDIVISION;
    for (float x=std::floor(r.left()); x<=r.right(); x++)
      for (int i=0; i<S; i++)
        painter->drawLine(QPointF(x + i/S, r.top()),
                          QPointF(x + i/S, r.bottom()));

    for (float y=std::floor(r.top()); y<=r.bottom(); y++)
      for (int i=0; i<S; i++)
        painter->drawLine(QPointF(r.left(), y + i/S),
                          QPointF(r.right(), y + i/S));

  painter->restore();
}

void Environment::paint (QPainter *painter,
                         const QStyleOptionGraphicsItem *options, QWidget*) {
  painter->fillRect(boundingRect(), Qt::white);

  if (config::Visualisation::renderingType() == RenderingType::VISION)
    drawVisionUnderlay(painter, options->rect);
}

} // end of namespace visu
