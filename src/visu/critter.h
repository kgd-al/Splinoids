#ifndef VISU_CRITTER_H
#define VISU_CRITTER_H

#include <QGraphicsItem>

#include "../simu/critter.h"

namespace visu {

class Critter : public QGraphicsItem {
public:
  static constexpr auto SPLINES_COUNT = genotype::Critter::SPLINES_COUNT;

private:
  simu::Critter &_critter;
  QPainterPath _body;
  std::array<QPainterPath, SPLINES_COUNT> _artifacts;

#ifndef NDEBUG
  QVector<QPolygonF> _polygons;
#endif

  QRectF _maximalBoundingRect;

public:
  Critter(simu::Critter &critter);

  const auto& object (void) const {
    return _critter;
  }

  auto& object (void) {
    return _critter;
  }

  void updatePosition (void);
  void updateShape (void);

  QRectF boundingRect (void) const;
  QRectF maximalBoundingRect (void) const {
    return _maximalBoundingRect;
  }

  void paint(QPainter *painter,
             const QStyleOptionGraphicsItem*, QWidget *) override;
  void doPaint (QPainter *painter) const;

  void save (QString filename = QString()) const;

private:

#ifndef NDEBUG
  void debugDrawBelow (QPainter *painter) const;
  void debugDrawAbove (QPainter *painter) const;
#else
  void debugDrawBelow (QPainter*) const {}
  void debugDrawAbove (QPainter*) const {}
#endif
};

} // end of namespace visu

#endif // VISU_CRITTER_H
