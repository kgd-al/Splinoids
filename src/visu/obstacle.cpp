#include "obstacle.h"

#include "config.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QPainterPath>

#include "box2d/b2_fixture.h"
#include "box2d/b2_polygon_shape.h"

namespace visu {

Obstacle::Obstacle(b2Body &body) : _obstacle(body) {
  setAcceptedMouseButtons(Qt::NoButton);
  setPos(toQt(body.GetPosition()));

  b2Fixture *f = body.GetFixtureList();
  while (f) {
    const b2PolygonShape *s =
        dynamic_cast<const b2PolygonShape*>(f->GetShape());
    auto n = s->m_count;
    auto v = s->m_vertices;
    QPolygonF p;
    for (int i=0; i<n; i++) p << toQt(v[i]);
    _body.addPolygon(p);

    f = f->GetNext();
  }

  setZValue(-10);
}

QRectF Obstacle::boundingRect (void) const {
  return _body.boundingRect();
}

void Obstacle::paint(QPainter *painter,
                  const QStyleOptionGraphicsItem*, QWidget*) {
  painter->setPen(Qt::NoPen);
  painter->fillPath(_body, Qt::black);
}

} // end of namespace visu
