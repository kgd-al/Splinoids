#include "obstacle.h"

#include "config.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QPainterPath>

#include "../simu/environment.h"
//#include "box2d/b2_fixture.h"
//#include "box2d/b2_polygon_shape.h"

namespace visu {

Obstacle::Obstacle(simu::Obstacle *o) : _obstacle(*o) {
  setAcceptedMouseButtons(Qt::NoButton);
  setPos(toQt(o->body().GetPosition()));

  b2Fixture *f = o->body().GetFixtureList();
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

  if (_obstacle.color() == config::Simulation::obstacleColor())
    _color == Qt::black;
  else
    _color = toQt(_obstacle.color());

  setZValue(-10);
}

QRectF Obstacle::boundingRect (void) const {
  return _body.boundingRect();
}

void Obstacle::paint(QPainter *painter,
                     const QStyleOptionGraphicsItem*, QWidget*) {
  painter->setPen(Qt::NoPen);
  painter->fillPath(_body, _color);
}

} // end of namespace visu
