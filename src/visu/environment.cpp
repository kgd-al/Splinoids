#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <QDebug>
#include <QGraphicsScene>

#include "environment.h"

#include "config.h"
#include "b2debugdrawer.h"

namespace visu {

Environment::Environment (simu::Environment &e) : _environment(e) {
  setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
  setZValue(-20);

  _ddrawer = new DebugDrawer;
  _environment.physics().SetDebugDraw(_ddrawer);
  _ddrawer->SetFlags(
      b2Draw::e_shapeBit
//    | b2Draw::e_aabbBit
    | b2Draw::e_pairBit
//    | b2Draw::e_centerOfMassBit
  );
}

Environment::~Environment (void) {
  delete _ddrawer;
}

void drawVisionUnderlay (QPainter *painter, const QRectF &r) {
  painter->save();
    QPen pen = painter->pen();
    pen.setWidthF(0);
    pen.setColor(Qt::lightGray);
    painter->setPen(pen);

    for (float x=std::floor(r.left()); x<=r.right(); x++)
      painter->drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));

    for (float y=std::floor(r.top()); y<=r.bottom(); y++)
      painter->drawLine(QPointF(r.left(), y), QPointF(r.right(), y));

  painter->restore();
}

void Environment::paint (QPainter *painter,
                         const QStyleOptionGraphicsItem *options, QWidget*) {
  painter->save();
    QPen pen = painter->pen();
    pen.setColor(Qt::black);
    pen.setWidth(1);
    painter->setPen(pen);
    const b2ChainShape *edgeVertices =
      dynamic_cast<const b2ChainShape*>(
        _environment.body()->GetFixtureList()->GetShape());
    auto n = edgeVertices->m_count-1;
    auto v = edgeVertices->m_vertices;
    for (int i=0; i<n; i++) painter->drawLine(toQt(v[i]), toQt(v[(i+1)%n]));
  painter->restore();

  painter->fillRect(simuBoundingRect(), Qt::white);

//  if (config::Visualisation::renderingType() == RenderingType::VISION)
    drawVisionUnderlay(painter, options->rect.intersected(simuBoundingRect().toAlignedRect()));
}

#ifndef NDEBUG
void Environment::doDebugDraw(void) {
  _ddrawer->clear();
  if (config::Visualisation::b2DebugDraw()) _environment.physics().DebugDraw();
}
#endif

} // end of namespace visu
