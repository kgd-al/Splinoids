#include <QPainter>

#include "b2debugdrawer.h"
#include "config.h"

namespace visu {

QColor toQt (const b2Color &c) {
  return QColor::fromRgbF(c.r, c.g, c.b, c.a);
}

void DebugDrawer::b2DD::draw (QPainter *p) const {
  prePaint(p);
    doPaint(p);
  postPaint(p);
}

void DebugDrawer::b2DD::prePaint (QPainter *p) const {
  p->save();
  QPen pen = p->pen();
  pen.setWidth(0);
  if (!filled)  pen.setColor(color);
  p->setPen(pen);
}

void DebugDrawer::b2DD::postPaint (QPainter *p) const {
  p->restore();
}

struct b2DDPolygon : public DebugDrawer::b2DD {
  QPainterPath polygon;

  b2DDPolygon (const b2Vec2 *v, int32 n, const QColor &c, bool f)
    : b2DD (c, f) {
    QPolygonF p;
    for (int i=0; i<n; i++) p << toQt(v[i]);
    polygon.addPolygon(p);
    polygon.closeSubpath();
  }

  void doPaint (QPainter *p) const override {
    if (filled) p->fillPath(polygon, color);
    p->drawPath(polygon);
  }
};

struct b2DDCircle : public DebugDrawer::b2DD {
  QPainterPath circle;

  b2DDCircle (const b2Vec2 &p, float r, const QColor &c, bool f)
    : b2DD(c, f){
    circle.addEllipse(toQt(p), r, r);
  }

  void doPaint (QPainter *p) const override {
    if (filled) p->fillPath(circle, color);
    p->drawPath(circle);
  }
};

struct b2DDLine : public DebugDrawer::b2DD {
  QLineF line;

  b2DDLine (const b2Vec2 &p1, const b2Vec2 &p2, const QColor &c)
    : b2DD(c, false) {
    line = { toQt(p1), toQt(p2) };
  }

  void doPaint (QPainter *p) const override {
    p->drawLine(line);
  }
};

struct b2DDPoint : public DebugDrawer::b2DD {
  QPointF point;

  b2DDPoint (const b2Vec2 &p, const QColor &c) : b2DD(c, false) {
    point = toQt(p);
  }

  void doPaint (QPainter *p) const override {
    p->drawPoint(point);
  }
};

DebugDrawer::DebugDrawer (int zvalue, float alpha) : alpha(alpha) {
  setZValue(zvalue);
}
DebugDrawer::~DebugDrawer(void) {}

QColor DebugDrawer::makeColor(const b2Color &c) const {
  return QColor::fromRgbF(c.r, c.g, c.b, c.a * alpha);
}

void DebugDrawer::DrawPolygon(const b2Vec2* vertices, int32 vertexCount,
                              const b2Color& color) {
  _draws.push_back(std::make_unique<b2DDPolygon>(vertices, vertexCount,
                                                 makeColor(color), false));
}

void DebugDrawer::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount,
                                   const b2Color& color) {
  _draws.push_back(std::make_unique<b2DDPolygon>(vertices, vertexCount,
                                                 makeColor(color), true));
}

void DebugDrawer::DrawCircle(const b2Vec2& center, float radius,
                             const b2Color& color) {
  _draws.push_back(std::make_unique<b2DDCircle>(center, radius,
                                                makeColor(color), false));
}

void DebugDrawer::DrawSolidCircle(const b2Vec2& center, float radius,
                                  const b2Vec2& /*axis*/, const b2Color& color) {
  _draws.push_back(std::make_unique<b2DDCircle>(center, radius,
                                                makeColor(color), true));
  using simu::operator<<;
//  std::cerr << "[b2DD] Axis of solid circle: " << axis << std::endl;
}

void DebugDrawer::DrawSegment(const b2Vec2& p1, const b2Vec2& p2,
                              const b2Color& color) {
  _draws.push_back(std::make_unique<b2DDLine>(p1, p2, makeColor(color)));
}

void DebugDrawer::DrawTransform(const b2Transform& xf) {
//  std::cerr << "[b2DD] Ignoring transforms" << std::endl;
  _draws.push_back(std::make_unique<b2DDLine>(b2Vec2{0,0}, b2Mul(xf, {1,0}),
                                              makeColor({0,1,0})));

  _draws.push_back(std::make_unique<b2DDLine>(b2Vec2{0,0}, b2Mul(xf, {0,1}),
                                              makeColor({0,0,1})));
}

void DebugDrawer::DrawPoint(const b2Vec2& p, float /*size*/, const b2Color& color) {
  _draws.push_back(std::make_unique<b2DDPoint>(p, makeColor(color)));
//  std::cerr << "[b2DD] Size of point: " << size << std::endl;
}

QRectF DebugDrawer::boundingRect(void) const {
  return QRectF(-50, -50, 100, 100);
}

void DebugDrawer::paint (QPainter *p,
            const QStyleOptionGraphicsItem*, QWidget*) {
  for (const auto &d: _draws) d->draw(p);
}

} // end of namespace visu
