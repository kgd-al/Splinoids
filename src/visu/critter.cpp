#include "critter.h"

#include "config.h"

#include <QVector2D>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QPainterPath>
#include <QtMath>
#include <QFileDialog>

#include <QtPrintSupport/QPrinter>

#include "box2d/b2_fixture.h"
#include "box2d/b2_circle_shape.h"
#include "box2d/b2_polygon_shape.h"

#include <QDebug>

namespace visu {

static constexpr bool drawMotors = true;
static constexpr bool drawVelocity = false;

using Sex = simu::Critter::Sex;
static const QColor artifactsFillColor = QColor::fromRgbF(.9, .85, .75);
static const QColor bodyFillColor1 = QColor::fromRgbF(.0, .0, .0);
static const QColor bodyFillColor2 = QColor::fromRgbF(.0, .0, .0, .0);
static const QColor visionFillColor = QColor::fromRgbF(1, .5, .5, .125);
static const QMap<Sex, QColor> sexColors {
  {   Sex::MALE, QColor::fromRgbF( .6, .6, 1. ) },
  { Sex::FEMALE, QColor::fromRgbF(1.,  .6,  .6) },
};

using Side = simu::Critter::Side;

QRectF toQt (const b2AABB &aabb) {
  return QRectF(toQt(aabb.lowerBound), toQt(aabb.upperBound));
}

Critter::Critter(simu::Critter &critter)
  : _critter(critter), _prevState(critter) {

  setFlag(ItemIsSelectable, true);
  setAcceptedMouseButtons(Qt::NoButton);
  setZValue(2);
  updatePosition();
  updateShape();
}

void Critter::updatePosition (void) {
  setPos(_critter.x(), _critter.y());
}

QPointF fromPolar (float angle, float length) {
  return length * QPointF(std::cos(angle), std::sin(angle));
}

QRectF squarify (const QRectF &r) {
  QPointF c = r.center();
  qreal s = std::max(
    std::max(r.right() - c.x(), c.x() - r.left()),
    std::max(r.top() - c.y(), c.y() - r.bottom())
  );
  return QRectF (c-QPointF(s, s), QSizeF(2*s, 2*s));
}

void Critter::update (void) {
  CritterState newState (_critter);

  if (newState.pos != _prevState.pos) updatePosition();

  if (config::Visualisation::trace() > 0) trace.push_back(pos());

  bool changed = false;
  for (uint i=0; i<_prevState.masses.size() && !changed; i++)
    if (_prevState.masses[i] != newState.masses[i])
      changed = true;
  for (uint i=0; i<_prevState.health.size() && !changed; i++)
    if (_prevState.health[i] != newState.health[i])
      changed = true;

  if (changed) {
    updateShape();
    emit shapeChanged();
  }

  QGraphicsItem::update();
}

b2AABB Critter::computeb2ABBB (void) const {
  // Compute b2AABB
  b2Transform t;
  t.SetIdentity();
  b2AABB aabb;
  aabb.lowerBound.Set( FLT_MAX,  FLT_MAX);
  aabb.upperBound.Set(-FLT_MAX, -FLT_MAX);
  const b2Fixture *f = _critter.fixturesList();
  while (f) {
    if (!f->IsSensor()) {
      const b2Shape *s = f->GetShape();
      for (int c=0; c<s->GetChildCount(); c++) {
        b2AABB sAABB;
        s->ComputeAABB(&sAABB, t, c);
        aabb.Combine(sAABB);
      }
    }
    f = f->GetNext();
  }

  return aabb;
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

  QRectF b2br = computeAABB();

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

  float visionRange = _critter.bodyRadius() + _critter.visionRange(),
        auditionRange = _critter.bodyRadius()
                        * config::Simulation::auditionRange(),
        reproRange = _critter.bodyRadius()
                        * config::Simulation::reproductionRange();
  float rmax = std::max(visionRange, std::max(auditionRange, reproRange));
  QRectF otherBB (-rmax, -rmax, 2*rmax, 2*rmax);

  _maximalBoundingRect = squarify(_critterBoundingRect.united(otherBB));
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
  painter->save();
  painter->rotate(qRadiansToDegrees(_critter.rotation()));
  doPaint(painter);
  painter->restore();

  if (!trace.isEmpty()) {
    QPointF currPos = pos();
    float s = (tag == "S" ? .66 : .33);
    int step = std::max(1, trace.size() / config::Visualisation::trace());
    for (int i=0; i<trace.size(); i+= step) {
      QColor c = bodyColor();
      c.setAlphaF(.5*float(i) / trace.size());
      painter->save();
        painter->translate(trace[i].x() - currPos.x(),
                           trace[i].y() - currPos.y());

        painter->scale(s, s);

        painter->fillPath(_body, c);
      painter->restore();
//      qDebug() << i << trace[i] << pos() <<
    }
    setPos(currPos);
  }

  if (!tag.isEmpty()) {
    painter->save();
    painter->scale(1, -1);

    painter->setPen(Qt::black);
    auto rect = boundingRect().translated(0, .25);
    QFont f = painter->font();
    f.setWeight(QFont::Bold);
    f.setPointSizeF(.1*f.pointSizeF());
    painter->setFont(f);

    painter->drawText(rect, Qt::AlignCenter, tag);
    painter->restore();
  }
}

QColor Critter::bodyColor(void) const {
  switch (config::Visualisation::renderType()) {
  case RenderingType::REGULAR:  return toQt(_critter.currentBodyColor());
  case RenderingType::ENERGY: {
    float r = float(_critter.usableEnergy() / _critter.maxUsableEnergy());
    return QColor::fromRgbF(r, r, 0);
  }
  case RenderingType::HEALTH:
    return QColor::fromRgbF(_critter.bodyHealthness(), 0, 0);
  }
  assert(false);
  return QColor();
}

QColor Critter::splineColor(uint i, Side s) const {
  switch (config::Visualisation::renderType()) {
  case RenderingType::REGULAR:  return toQt(_critter.currentSplineColor(i,s));
  case RenderingType::ENERGY:   return QColor(Qt::black);
  case RenderingType::HEALTH:
    return QColor::fromRgbF(_critter.splineHealthness(i, s), 0, 0);
  }
  assert(false);
  return QColor();
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

    if (isSelected() && false) {
      pen.setColor(Qt::gray); pen.setStyle(Qt::DotLine); painter->setPen(pen);
      painter->drawRect(boundingRect());
      pen.setColor(Qt::red); pen.setStyle(Qt::DashLine); painter->setPen(pen);
      painter->drawRect(critterBoundingRect());
    }

    if (config::Visualisation::drawAudition() && isSelected()) {
      const b2Fixture *as = _critter.auditionSensor();
      if (as) {
        float r = dynamic_cast<const b2CircleShape*>(as->GetShape())->m_radius;
        painter->save();
        painter->setBrush(QColor::fromRgbF(0,0,1,.1));
        painter->drawEllipse(QPointF(0,0), r, r);
        painter->restore();
      }
    }

    if (config::Visualisation::drawReproduction()) {
      const b2Fixture *rs = _critter.reproductionSensor();
      if (rs) {
        float r = dynamic_cast<const b2CircleShape*>(rs->GetShape())->m_radius;
        painter->save();
        painter->setBrush(QColor::fromRgbF(1,0,0,.1));
        painter->drawEllipse(QPointF(0,0), r, r);
        painter->restore();
      }
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

        if (!config::Visualisation::ghostMode()) {
          if (config::Visualisation::opaqueBodies())
            painter->fillPath(_body, bodyColor());

          if (!config::Visualisation::drawInnerEdges())
            for (uint i=0; i<SPLINES_COUNT; i++)
              doSymmetrical(painter, [this, i] (QPainter *p, Side s) {
                if (shouldDrawSpline(i, s))
                  p->drawPath(_artifacts[i]);
              });

          if (config::Visualisation::opaqueBodies())
            for (uint i=0; i<SPLINES_COUNT; i++)
              doSymmetrical(painter, [this, i] (QPainter *p, Side s) {
                if (shouldDrawSpline(i, s))
                    p->fillPath(_artifacts[i], splineColor(i, s));
              });

          if (config::Visualisation::drawInnerEdges())
            doSymmetrical(painter, [this] (QPainter *p, Side s) {
              for (uint i=0; i<SPLINES_COUNT; i++)
                if (shouldDrawSpline(i, s))
                  p->drawPath(_artifacts[i]);
            });

          if (!config::Visualisation::opaqueBodies()) {
            pen.setWidthF(0);
            pen.setColor(Qt::black);
            painter->setPen(pen);
            painter->drawPath(_body);
          }
        }

      painter->restore();

      debugDrawAbove(painter);

//      pen.setStyle(Qt::DashLine);
//      pen.setColor(Qt::red);
//      painter->setPen(pen);
//      painter->drawRect(_artifacts.boundingRect());
    painter->restore();

    static const auto &DV = config::Visualisation::drawVision();
    if (DV && (DV >= 3 || isSelected())) {
      painter->save();
        pen.setColor(visionFillColor.darker());
        pen.setStyle(Qt::SolidLine);
        painter->setPen(pen);
        painter->setBrush(visionFillColor);
        doSymmetrical(painter, [this] (QPainter *p, Side) {
          p->drawPath(_visionCone);
        });

        if (DV % 2 == 0) {
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

//    float e = _critter.sizeRatio();
//    pen.setColor(sexColors.value(_critter.sex()));
//    pen.setStyle(Qt::SolidLine);
//    pen.setJoinStyle(Qt::RoundJoin);
//    pen.setWidthF(.05 * e);
//    painter->setPen(pen);
//    painter->drawPolyline(
//      QPolygonF({ QPointF(0., -.1*e), QPointF(.1*e, 0.), QPointF(0.,  .1*e) }));
  painter->restore();


//  qDebug() << "Painted " << CID(this) << "with size " << _critter.size();
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

        if (d.test(i+2*SPLINES_COUNT)) {  // Draw edges
          pen.setStyle(Qt::DashLine);  pen.setColor(Qt::gray);  painter->setPen(pen);
          painter->drawPolyline(QPolygonF({
            toQt(s.pl0), toQt(s.cl0), toQt(s.cl1),
            toQt(s.p1),
            toQt(s.cr0), toQt(s.cr1), toQt(s.pr0)
          }));
        }

        if (d.test(i+SPLINES_COUNT)) {  // Draw central spline
          pen.setStyle(Qt::DashLine);  pen.setColor(Qt::gray);  painter->setPen(pen);
          QPainterPath p; // central
          p.moveTo(toQt(s.p0));
          p.cubicTo(toQt(s.c0), toQt(s.c1), toQt(s.p1));
          painter->drawPath(p);
          p = QPainterPath(); // left
          p.moveTo(toQt(s.pl0));
          p.cubicTo(toQt(s.cl0), toQt(s.cl1), toQt(s.p1));
          painter->drawPath(p);
          p = QPainterPath(); // right
          p.moveTo(toQt(s.pr0));
          p.cubicTo(toQt(s.cr0), toQt(s.cr1), toQt(s.p1));
          painter->drawPath(p);
        }

        if (d.test(i)) {  // Draw control points
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

  if (drawMotors) {
  painter->save();
    float e = _critter.sizeRatio();
    float mo_W = .1*e;
    float R = object().bodyRadius();
    float offset = .5 * R;
    float length = .2 * R;

    pen.setStyle(Qt::DotLine);
    painter->setPen(pen);
    painter->setBrush(Qt::black);

    for (Motor m: EnumUtils<Motor>::iterator()) {
      float o = object().motorOutput(m);
      QRectF r (0, int(m) * offset-.5*mo_W, length * o, mo_W);
      painter->fillRect(r, QColor::fromRgbF(0, 0, 0, std::fabs(o)));
//      painter->drawRect(QRectF(-length, -int(m) * offset-.5*mo_W, 2*length, mo_W));
    }
  painter->restore();
  }

  if (drawVelocity) {
  painter->save();
    pen.setColor(Qt::red);
    painter->setPen(pen);
    painter->drawLine({0,0},
                      toQt(
                        object().body().GetLocalVector(
                          object().body().GetLinearVelocity())));
  painter->restore();
  }
}
#endif

void Critter::saveGenotype(const QString &filename) const {
  _critter.genotype().toFile(filename.toStdString());
}

void Critter::printPhenotype (const QString &filename, int size) const {
  if (filename.isEmpty())
    return;
  else if (filename.endsWith(".png"))
    printPhenotypePng(filename, size);
  else
    qDebug() << "Unmanaged extension for filename" << filename;
}

QPixmap Critter::renderPhenotype(int size) const {
  float S = size > 0 ? size : 10*config::Visualisation::viewZoom();
  const QRectF &r = _critterBoundingRect;
  float R = S / std::max(r.width(), r.height());
  float W = r.width() * R, H = r.height() * R;

  QPixmap pixmap (W, H);
  pixmap.fill(Qt::transparent);

  QPainter painter (&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.translate(.5*W, .5*H);
  painter.rotate(-90);
  painter.scale(.5*S, .5*S);
  doPaint(&painter);

  qDebug() << "[SAVE]" << S << r << R << W << H;

  return pixmap;
}

void Critter::printPhenotypePng (const QString &filename, int size) const {
  QPixmap pixmap = renderPhenotype(size);
//  qDebug() << "Critter bounding rects are:";
//  qDebug() << "\tminimal: " << _minimalBoundingRect;
//  qDebug() << "Pixmap size is" << pixmap.size();
  bool ok = pixmap.save(filename);
  qDebug().nospace() << (ok ? "Saved" : " Failed to save") << " C"
                     << uint(_critter.id()) << " to " << filename;
}

QDebug operator<<(QDebug dbg, const CID &cid) {
  dbg.nospace() << cid.prefix << cid.c.firstname();
  return dbg.maybeSpace();
}


} // end of namespace visu
