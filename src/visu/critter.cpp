#include "critter.h"

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

using Sex = simu::Critter::Sex;
static const QColor artifactsFillColor = QColor::fromRgbF(.9, .85, .75);
static const QColor bodyFillColor1 = QColor::fromRgbF(.0, .0, .0);
static const QColor bodyFillColor2 = QColor::fromRgbF(.0, .0, .0, .0);
static const QColor visionFillColor = QColor::fromRgbF(1, .5, .5, .125);
static const QMap<Sex, QColor> sexColors {
  {   Sex::MALE, QColor::fromRgbF( .6, .6, 1. ) },
  { Sex::FEMALE, QColor::fromRgbF(1.,  .6,  .6) },
};

Critter::Critter(simu::Critter &critter) : _critter(critter) {
  setFlag(ItemIsSelectable, true);
  setAcceptedMouseButtons(Qt::NoButton);
  setZValue(2);
  updatePosition();
  updateShape();

  _bcolor = toQt(critter.bodyColor());
  for (uint i=0; i<SPLINES_COUNT; i++)
    _acolors[i] = toQt(critter.splineColor(i));
}


void Critter::updatePosition (void) {
  setPos(_critter.x(), _critter.y());
}

QPointF fromPolar (float angle, float length) {
  return length * QPointF(std::cos(angle), std::sin(angle));
}

QRectF squarify (const QRectF &r) {
  qreal s = std::max(
    std::max(r.right(), -r.left()),
    std::max(r.top(), -r.bottom())
  );
  return QRectF (-s, -s, 2*s, 2*s);
}

void Critter::updateShape (void) {
  prepareGeometryChange();

  float r = _critter.bodyRadius();

  _body = QPainterPath();
  _body.addEllipse(QPoint(0, 0), r, r);

  QRectF abr;
  _artifacts.fill(QPainterPath());
  const auto &splines = _critter.splinesData();
  for (uint i=0; i<SPLINES_COUNT; i++) {
    const auto &s = splines[i];
    auto &p = _artifacts[i];
    p.moveTo(toQt(s.pl0));
    p.cubicTo(toQt(s.cl0), toQt(s.cl1), toQt(s.p1));
    p.cubicTo(toQt(s.cr0), toQt(s.cr1), toQt(s.pr0));
    p.arcTo(-r, -r, 2*r, 2*r,
            -qRadiansToDegrees(s.ar0),
            -qRadiansToDegrees(s.al0-s.ar0));
    p.closeSubpath();
    p.setFillRule(Qt::WindingFill);
    abr = abr.united(p.boundingRect());
  }

  updateVision();

  QRectF b2br;
  { // Compute b2AABB
    b2Transform t;
    t.SetIdentity();
    b2AABB aabb;
    aabb.lowerBound.Set( FLT_MAX,  FLT_MAX);
    aabb.upperBound.Set(-FLT_MAX, -FLT_MAX);
    const b2Fixture *f = _critter.fixturesList();
    while (f) {
      const b2Shape *s = f->GetShape();
      for (int c=0; c<s->GetChildCount(); c++) {
        b2AABB sAABB;
        s->ComputeAABB(&sAABB, t, c);
//        shapeAABB.lowerBound = shapeAABB.lowerBound;
//        shapeAABB.upperBound = shapeAABB.upperBound;
        aabb.Combine(sAABB);
      }
      f = f->GetNext();
    }
    b2br = QRectF(toQt(aabb.lowerBound), toQt(aabb.upperBound));
  }

#ifndef NDEBUG
  _polygons.clear();
  for (const auto &v: _critter.collisionObjects) {
    for (const auto &s: v) {
      QPolygonF p;
      for (const auto &v: s)  p << toQt(v);
      _polygons.push_back(p);
    }
  }
  _b2polygons.clear();  uint fixtures = 0;
  const b2Fixture *f = _critter.fixturesList();
  while (f) {
    const b2Shape *s = f->GetShape();
    b2Shape::Type t = s->GetType();
    if (t == b2Shape::e_polygon) {
      const b2PolygonShape *bp = dynamic_cast<const b2PolygonShape*>(s);
      QPolygonF qp;
      for (int i=0; i<bp->m_count; i++)  qp << toQt(bp->m_vertices[i]);
      _b2polygons.push_back(qp);
    }
    f = f->GetNext();
    fixtures++;
  }
#endif

  QRectF abr_f = QRectF(abr.x(), -abr.y() -abr.height(),
                        abr.width(), abr.height());

  _minimalBoundingRect = _body.boundingRect().united(b2br)
                                            .united(abr).united(abr_f);

  _critterBoundingRect = squarify(_minimalBoundingRect);

  QRectF vcbr = _visionCone.boundingRect(),
         vcbr_f = QRectF(vcbr.x(), -vcbr.y() - vcbr.height(),
                         vcbr.width(), vcbr.height());

  _maximalBoundingRect =
    squarify(_critterBoundingRect.united(vcbr).united(vcbr_f));
}

void Critter::updateVision(void) {
  const genotype::Vision &v = _critter.genotype().vision;
  _visionCone = QPainterPath();

  float a0 = v.angleBody;
  QPointF pB = _critter.bodyRadius() * QPointF(std::cos(a0), std::sin(a0));

  float dar = v.angleRelative - .5 * v.width;
  float ar = a0 + dar;
  float r = _critter.visionRange();
  QPointF pr = pB + r * QPointF(std::cos(ar), std::sin(ar));

  _visionCone.moveTo(pB);
  _visionCone.lineTo(pr);
  _visionCone.arcTo(QRectF(pB - QPointF(r,r), pB + QPointF(r, r)),
                    -qRadiansToDegrees(ar), -qRadiansToDegrees(v.width));
  _visionCone.lineTo(pB);
}

QRectF Critter::boundingRect (void) const {
  int S = _critter.bodyRadius();
  return QRectF(-S, -S, 2*S, 2*S).united(_maximalBoundingRect);
}

void Critter::paint(QPainter *painter,
                    const QStyleOptionGraphicsItem*, QWidget*) {
  painter->rotate(qRadiansToDegrees(_critter.rotation()));
  doPaint(painter);
}

QColor Critter::bodyColor(void) const {
  switch (config::Visualisation::renderType()) {
  case RenderingType::REGULAR:  return _bcolor;
  case RenderingType::ENERGY: {
    float r = _critter.usableEnergy() / _critter.maxUsableEnergy();
    return QColor::fromRgbF(r, r, 0);
  }
  case RenderingType::HEALTH: {
    float r = _critter.bodyHealth() / _critter.bodyMaxHealth();
    return QColor::fromRgbF(r, 0, 0);
  }
  }
  assert(false);
}

QColor Critter::splineColor(uint i, Side s) const {
  switch (config::Visualisation::renderType()) {
  case RenderingType::REGULAR:  return _acolors[i];
  case RenderingType::ENERGY:   return QColor(Qt::black);
  case RenderingType::HEALTH: {
    float r = _critter.splineHealth(i, s) / _critter.splineMaxHealth(i, s);
    return QColor::fromRgbF(r, 0, 0);
  }
  }
  assert(false);
}

bool Critter::shouldDrawSpline(uint i, Side s) const {
  if (_critter.destroyedSpline(i, s)) return false;
  switch (config::Visualisation::renderType()) {
  case RenderingType::REGULAR:  return true;
  case RenderingType::ENERGY:   return false;
  case RenderingType::HEALTH:   return _critter.activeSpline(i, s);
  }
  return true;
}

static const auto doSymmetrical = [] (QPainter *p, auto f) {
  p->save();
    f(p, Critter::Side::LEFT);
    p->scale(1, -1);
    f(p, Critter::Side::RIGHT);
  p->restore();
};

void Critter::doPaint (QPainter *painter) const {
#ifndef NDEBUG
//  if (config::Visualisation::b2DebugDraw()) return;
#endif

  painter->save();
    QPen pen = painter->pen();
    pen.setWidthF(0);
    painter->setPen(pen);

    if (isSelected()) {
      pen.setColor(Qt::gray); pen.setStyle(Qt::DotLine); painter->setPen(pen);
      painter->drawRect(boundingRect());
      pen.setColor(Qt::red); pen.setStyle(Qt::DashLine); painter->setPen(pen);
      painter->drawRect(critterBoundingRect());
    }

//    pen.setColor(Qt::red);
//    painter->setPen(pen);
//    painter->drawRect(boundingRect());

//    pen.setColor(Qt::blue);
//    painter->setPen(pen);
//    painter->drawEllipse(QPointF(0, 0), .5*_critter.size(), .5*_critter.size());

    painter->save();    
      pen.setColor(Qt::black);  pen.setStyle(Qt::SolidLine); painter->setPen(pen);
      debugDrawBelow(painter);

      painter->save();
//        pen.setColor(Qt::gray);
//        pen.setStyle(Qt::DashLine);
//        painter->setPen(pen);
//        painter->drawPath(_debug);
//        pen.setColor(Qt::black);
//        pen.setStyle(Qt::SolidLine);
//        painter->setPen(pen);

        if (config::Visualisation::opaqueBodies())
          painter->fillPath(_body, bodyColor());

        if (!config::Visualisation::drawInnerEdges())
          doSymmetrical(painter, [this] (QPainter *p, Side s) {
            for (uint i=0; i<SPLINES_COUNT; i++)
              if (shouldDrawSpline(i, s))
                p->drawPath(_artifacts[i]);
          });

        doSymmetrical(painter, [this] (QPainter *p, Side s) {
          for (uint i=0; i<SPLINES_COUNT; i++)
            if (shouldDrawSpline(i, s))
              p->fillPath(_artifacts[i], splineColor(i, s));
        });

        if (config::Visualisation::drawInnerEdges())
          doSymmetrical(painter, [this] (QPainter *p, Side s) {
            for (uint i=0; i<SPLINES_COUNT; i++)
              if (shouldDrawSpline(i, s))
                p->drawPath(_artifacts[i]);
          });
      painter->restore();

      debugDrawAbove(painter);

//      pen.setStyle(Qt::DashLine);
//      pen.setColor(Qt::red);
//      painter->setPen(pen);
//      painter->drawRect(_artifacts.boundingRect());
    painter->restore();

    if (config::Visualisation::opaqueBodies())
      painter->fillPath(_body, bodyFillColor2);

    else {
      pen.setWidthF(0);
      pen.setColor(Qt::black);
      painter->setPen(pen);
      painter->drawPath(_body);
    }

    if (config::Visualisation::drawVision() >= 1 && isSelected()) {
      painter->save();
        pen.setColor(visionFillColor.darker());
        pen.setStyle(Qt::SolidLine);
        painter->setPen(pen);
        painter->setBrush(visionFillColor);
        doSymmetrical(painter, [this] (QPainter *p, Side) {
          p->drawPath(_visionCone);
        });

        if (config::Visualisation::drawVision() >= 2) {
          const auto &s = object().raysStart();
          const auto &e = object().raysEnd();
          const auto &l = object().raysLength();
          const auto &r = object().retina();
          const auto n = e.size(), h = n/2;
          for (uint i=0; i<n; i++) {
            QPointF pstart = toQt(s[i<h?0:1]),
                    pmax = toQt(e[i]),
                    pend = pstart + l[i] * (pmax-pstart);
            QColor color = toQt(r[i]);
            pen.setColor(color);
            painter->setPen(pen);
            painter->drawLine(pstart, pend);
            painter->setBrush(color.lighter());
            painter->drawEllipse(pend, .0125, .0125);
          }
        }
      painter->restore();
    }

    pen.setColor(sexColors.value(_critter.sex()));
    pen.setStyle(Qt::SolidLine);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setWidthF(.05);
    painter->setPen(pen);
    painter->drawPolyline(
      QPolygonF({ QPointF(0., -.1), QPointF(.1, 0.), QPointF(0.,  .1) }));
  painter->restore();

//  qDebug() << "Painted critter " << uint(_critter.genotype().id())
//           << "with size " << _critter.size();
}

#ifndef NDEBUG
void Critter::debugDrawBelow (QPainter */*painter*/) const {

}

void Critter::debugDrawAbove (QPainter *painter) const {
  const auto &d = config::Visualisation::debugDraw;
  QPen pen = painter->pen();
  if (d.any()) {
    painter->save();
      for (uint i=0; i<SPLINES_COUNT; i++) {
        const auto &s = _critter.splinesData()[i];

        if (d.test(i+SPLINES_COUNT)) {
          pen.setStyle(Qt::DashLine);  pen.setColor(Qt::gray);  painter->setPen(pen);
          painter->drawPolyline(QPolygonF({
            toQt(s.pl0), toQt(s.cl0), toQt(s.cl1),
            toQt(s.p1),
            toQt(s.cr0), toQt(s.cr1), toQt(s.pr0)
          }));
        }

        if (d.test(i)) {
          pen.setStyle(Qt::SolidLine);  pen.setColor(Qt::black);  painter->setPen(pen);
          painter->drawLine(toQt(s.p0), toQt(s.p1));
          pen.setStyle(Qt::DashLine);  painter->setPen(pen);
          painter->drawLine(toQt(s.pc0), toQt(s.c0));
          painter->drawLine(toQt(s.pc1), toQt(s.c1));
          pen.setStyle(Qt::DotLine);  painter->setPen(pen);
          painter->drawPolyline(QPolygonF({
            toQt(s.p0), toQt(s.c0), toQt(s.c1), toQt(s.p1)
          }));
        }
      }
    painter->restore();
  }

  // Debug draw of simu spline objects
  const auto dco = config::Visualisation::showCollisionObjects();
  static constexpr float pr = .0125;
  if (dco) {
    painter->save();
      if (dco & 2) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(Qt::red);
        for (const auto &p: _b2polygons)
          for (const auto &p_: p)
            painter->drawEllipse(p_, pr, pr);
        painter->setBrush(Qt::darkRed);
        for (const auto &p: _polygons)
          for (const auto &p_: p)
            painter->drawEllipse(p_, .5*pr, .5*pr);
      }

      if (dco & 1) {
        painter->setBrush(Qt::NoBrush);
        pen.setStyle(Qt::DashLine);
        pen.setColor(Qt::green);
        painter->setPen(pen);
        for (const auto &p: _polygons)  painter->drawPolygon(p);
        pen.setColor(Qt::blue);
        painter->setPen(pen);
        for (const auto &p: _b2polygons)  painter->drawPolygon(p);
      }

      // Intermediate debug
//      painter->setBrush(Qt::darkRed);
//      painter->setPen(Qt::NoPen);
//      for (const auto &p: _critter._msIntersections)
//        painter->drawEllipse(toQt(p), .0125, .0125);

//      pen.setStyle(Qt::SolidLine);
//      pen.setColor(Qt::darkBlue);
//      painter->setPen(pen);
//      for (const auto &l: _critter._msLines)
//        painter->drawLine(toQt(l[0]), toQt(l[1]));

    painter->restore();
  }

  // draw motor outputs
  painter->save();
    static constexpr float mo_W = .1;
    float R = object().bodyRadius();
    float offset = .5 * R;
    float length = .2 * R;

    for (Motor m: EnumUtils<Motor>::iterator()) {
      float o = object().motorOutput(m);
      QRectF r (0, -int(m) * offset-.5*mo_W, length * o, mo_W);
      painter->fillRect(r, QColor::fromRgbF(1, 0, 0, std::fabs(o)));
    }
  painter->restore();

  // draw Velocity (physics)
  painter->save();
    pen.setColor(Qt::red);
    painter->setPen(pen);
    painter->drawLine({0,0},
                      toQt(
                        object().body().GetLocalVector(
                          object().body().GetLinearVelocity())));
  painter->restore();
}
#endif

void Critter::saveGenotype(const QString &filename) const {
  _critter.genotype().toFile(filename.toStdString());
}

void Critter::printPhenotype (const QString &filename) const {
  if (filename.isEmpty())
    return;

  static const float Z = config::Visualisation::viewZoom();
  QPixmap pixmap (boundingRect().size().toSize() * Z);
  pixmap.fill(Qt::transparent);
  QPainter painter (&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.translate(Z, Z);
  painter.rotate(-90);
  painter.scale(Z, Z);
  doPaint(&painter);
  pixmap.save(filename);
}

} // end of namespace visu
