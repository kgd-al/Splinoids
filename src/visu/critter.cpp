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
static const QMap<Sex, QColor> sexColors {
  {   Sex::MALE, QColor::fromRgbF( .6, .6, 1. ) },
  { Sex::FEMALE, QColor::fromRgbF(1.,  .6,  .6) },
};

Critter::Critter(simu::Critter &critter) : _critter(critter) {
  setFlag(ItemIsSelectable, true);
  setAcceptedMouseButtons(Qt::NoButton);
  updatePosition();
  updateShape();
}


void Critter::updatePosition (void) {
  setPos(_critter.x(), _critter.y());
}

QPointF fromPolar (float angle, float length) {
  return length * QPointF(std::cos(angle), std::sin(angle));
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

#ifndef NDEBUG
  _polygons.clear();
  for (const auto &s: _critter.collisionObjects) {
    QPolygonF p;
    for (const auto &v: s)  p << toQt(v);
    _polygons.push_back(p);
  }
  _b2polygons.clear();
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
  }
#endif

  QRectF abr_f = QRectF(abr.x(), -abr.y() -abr.height(),
                        abr.width(), abr.height());

//  qDebug() << "Expected bounding rect: " << boundingRect();
//  qDebug() << "    Body bounding rect: " << _body.boundingRect();
//  qDebug() << "     abr bounding rect: " << abr;
//  qDebug() << "   abr_f bounding rect: " << abr_f;

  _maximalBoundingRect = _body.boundingRect()
                          .united(abr)
                          .united(abr_f);

//  qDebug() << "Resulting bounding rect: " << _maximalBoundingRect;

  qreal s = std::max(_maximalBoundingRect.width(), _maximalBoundingRect.height());
  _maximalBoundingRect.setRect(-.5*s, -.5*s, s, s);

//  qDebug() << "Normalized bounding rect: " << _maximalBoundingRect;
}

QRectF Critter::boundingRect (void) const {
  int S = _critter.bodySize();
  return QRectF(-.5*S, -.5*S, S, S).united(_maximalBoundingRect);
}

void Critter::paint(QPainter *painter,
                    const QStyleOptionGraphicsItem*, QWidget*) {
  painter->rotate(qRadiansToDegrees(_critter.rotation()));
  doPaint(painter);
}

void Critter::doPaint (QPainter *painter) const {
  if (config::Visualisation::renderingType() != RenderingType::NORMAL)
    return;

  painter->save();
    QPen pen = painter->pen();
    pen.setWidthF(0);
    painter->setPen(pen);

    if (isSelected()) {
      pen.setColor(Qt::gray); pen.setStyle(Qt::DotLine); painter->setPen(pen);
      painter->drawRect(boundingRect());
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
          painter->fillPath(_body, bodyFillColor1);

        if (!config::Visualisation::drawInnerEdges()) {
          painter->scale(1,  1);
          for (const auto &a: _artifacts) painter->drawPath(a);

          painter->scale(1, -1);
          for (const auto &a: _artifacts) painter->drawPath(a);
        }

        painter->scale(1,  1);
        for (const auto &a: _artifacts) painter->fillPath(a, artifactsFillColor);

        painter->scale(1, -1);
        for (const auto &a: _artifacts) painter->fillPath(a, artifactsFillColor);

        if (config::Visualisation::drawInnerEdges()) {
          painter->scale(1,  1);
          for (const auto &a: _artifacts) painter->drawPath(a);

          painter->scale(1, -1);
          for (const auto &a: _artifacts) painter->drawPath(a);
        }
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
    painter->scale(1, 1);
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
    float S = object().bodySize();
    float R = .25 * S;
    float L = .1 * S;

    for (Motor m: EnumUtils<Motor>::iterator()) {
      float o = object().motorOutput(m);
      QRectF r (0, -int(m) * R-.5*mo_W, L * o, mo_W);
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

void Critter::save (QString filename) const {
  if (filename.isEmpty())
    filename =
      QFileDialog::getSaveFileName(nullptr, QString("Select filename"),
                                   QString(), QString("*.png"));
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
