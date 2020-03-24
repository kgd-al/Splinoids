#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <QDebug>
#include <QGraphicsScene>

#include "environment.h"
#include "critter.h"

#include "config.h"
#include "b2debugdrawer.h"

namespace visu {

struct guard_QPainter {
  QPainter *painter;
  guard_QPainter(QPainter *p) : painter(p) { painter->save(); }
  ~guard_QPainter(void) { painter->restore(); }
};

Environment::Environment (simu::Environment &e,
                          stvCritterMapping stvC,
                          stvFoodletMapping stvF)
    : _environment(e), stvCritter(stvC), stvFoodlet(stvF) {
  setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
  setZValue(-20);

  _overlay = new Overlay (*this, 20);
  _ddrawer = new DebugDrawer (10, .5);
  _environment.physics().SetDebugDraw(_ddrawer);
  _ddrawer->SetFlags(
      b2Draw::e_shapeBit
//    | b2Draw::e_aabbBit
    | b2Draw::e_pairBit
//    | b2Draw::e_centerOfMassBit
  );
}

Environment::~Environment (void) {
  delete _overlay;
  delete _ddrawer;
}

void drawVisionUnderlay (QPainter *painter, const QRectF &r) {
  painter->save();
    QPen pen = painter->pen();
    pen.setWidthF(0);
    pen.setColor(Qt::lightGray);
    painter->setPen(pen);

    for (int x=r.left(); x<=r.right(); x++)
      painter->drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));

    for (int y=r.top(); y<=r.bottom(); y++)
      painter->drawLine(QPointF(r.left(), y), QPointF(r.right(), y));

  painter->restore();
}

void Environment::paint (QPainter *painter,
                         const QStyleOptionGraphicsItem *options, QWidget*) {

  QRectF drawBounds = QRectF(options->rect).intersected(simuBoundingRect());

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

  if (config::Visualisation::showGrid())
    drawVisionUnderlay(painter, drawBounds);
}

#ifndef NDEBUG
void Environment::doDebugDraw(void) {
  _ddrawer->clear();
  if (config::Visualisation::b2DebugDraw()) _environment.physics().DebugDraw();
}
#endif

Overlay::Overlay (Environment &e, int zvalue) : env(e) {
  setZValue(zvalue);
}

void Overlay::paint (QPainter *painter,
                     const QStyleOptionGraphicsItem *options, QWidget*) {
  QRectF drawBounds = QRectF(options->rect).intersected(env.simuBoundingRect());

  if (config::Visualisation::showFights())
    drawFights(painter, drawBounds);
}

void Overlay::drawFights(QPainter *painter, const QRectF &drawBounds) const {
  int ddlevel = config::Visualisation::drawFightingDebug();
  if (ddlevel == 0) return;

  guard_QPainter gp (painter);

  QPen pen = painter->pen();
  pen.setColor(Qt::red);
  pen.setWidth(0);
  painter->setPen(pen);

  for (const auto &p: env._environment.fightingEvents()) {
    const visu::Critter *clhs = env.stvCritter(p.first.first);
    const QRectF &lhsBR = clhs->critterBoundingRect().translated(clhs->pos());
    if (!lhsBR.intersects(drawBounds))  continue;

    const visu::Critter *crhs = env.stvCritter(p.first.second);
    const QRectF &rhsBR = crhs->critterBoundingRect().translated(crhs->pos());
    if (!rhsBR.intersects(drawBounds))  continue;

    painter->drawRect(lhsBR.united(rhsBR));


  }

#ifndef NDEBUG
  if (ddlevel < 2)  return;

  for (const simu::Environment::FightingDrawData &fdd:
       env._environment.fightingDrawData) {

    QPointF pA = toQt(fdd.pA), pB = toQt(fdd.pB);
    if (!drawBounds.contains(pA.toPoint()))  continue;
    if (!drawBounds.contains(pB.toPoint())) continue;

    pen.setStyle(Qt::SolidLine);
    pen.setColor(Qt::blue);
    painter->setPen(pen);
    painter->drawPoint(pA);
    painter->drawPoint(pB);
    painter->drawLine(pA, pB);

    pen.setStyle(Qt::DotLine);
    pen.setColor(Qt::darkRed);
    painter->setPen(pen);
    painter->drawLine(pA, pA + toQt(fdd.vA));
    painter->drawLine(pB, pB + toQt(fdd.vB));

    pen.setColor(Qt::green);
    painter->setPen(pen);
    painter->drawLine(pA, pA + toQt(fdd.VA * fdd.C_ ));
    painter->drawLine(pB, pB - toQt(fdd.VB * fdd.C_));
  }
#endif
}

} // end of namespace visu
