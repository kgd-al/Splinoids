#ifndef VISU_CRITTER_H
#define VISU_CRITTER_H

#include <QGraphicsItem>

#include "../simu/critter.h"

namespace visu {

class Critter : public QObject, public QGraphicsItem {
  Q_OBJECT
public:
  static constexpr auto SPLINES_COUNT = genotype::Critter::SPLINES_COUNT;
  using Side = simu::Critter::Side;

private:
  simu::Critter &_critter;

  QPainterPath _body;
  std::array<QPainterPath, SPLINES_COUNT> _artifacts;

  QColor _bcolor;
  std::array<QColor, SPLINES_COUNT> _acolors;

  QPainterPath _visionCone;

#ifndef NDEBUG
  QVector<QPolygonF> _polygons, _b2polygons;
#endif

  QRectF _minimalBoundingRect, _critterBoundingRect, _maximalBoundingRect;

  struct CritterState {
#define DCY(X) std::decay_t<decltype(std::declval<simu::Critter>().X())>
    DCY(pos) pos;
    DCY(masses) masses;
    DCY(healthArray) health;
#undef DCY

    CritterState (const simu::Critter &c)
      : pos(c.pos()), masses(c.masses()), health(c.healthArray()) {}
  } _prevState;

public:
  QString tag;

  Critter(simu::Critter &critter);

  const auto& object (void) const {
    return _critter;
  }

  auto& object (void) {
    return _critter;
  }

  static constexpr auto NO_FIRSTNAME = "Unknown";
  QString firstname (void) const {
    using ut = std::underlying_type<phylogeny::GID>::type;
    auto gid = _critter.id();
    if (gid == phylogeny::GID::INVALID) return NO_FIRSTNAME;
    return "0x" + QString::number(ut(gid), 16);
  }

  static constexpr auto NO_LASTNAME = "Doe";
  QString lastname (void) const {
    using ut = std::underlying_type<phylogeny::SID>::type;
    auto sid = _critter.genotype().species();
    if (sid == phylogeny::SID::INVALID) return NO_LASTNAME;
    return "0x" + QString::number(ut(sid), 16);
  }

  void updatePosition (void);
  void updateShape (void);

  void update (void);

  QRectF boundingRect (void) const;
  QRectF sceneBoundingRect(void) const {
    return boundingRect().translated(pos());
  }

  QRectF minimalBoundingRect (void) const {
    return _minimalBoundingRect;
  }

  QRectF critterBoundingRect (void) const {
    return _critterBoundingRect;
  }

  QRectF maximalBoundingRect (void) const {
    return _maximalBoundingRect;
  }

  QPainterPath shape (void) const override {
    return _body;
  }

  void paint(QPainter *painter,
             const QStyleOptionGraphicsItem*, QWidget *) override;
  void doPaint (QPainter *painter) const;

  void saveGenotype (const QString &filename) const;

  QPixmap renderPhenotype (void) const;
  void printPhenotype (const QString &filename) const;

signals:
  void shapeChanged (void);

private:
  void updateVision (void);

  QColor bodyColor (void) const;
  QColor splineColor (uint i, simu::Critter::Side s) const;

  bool shouldDrawSpline (uint i, Side s) const;

#ifndef NDEBUG
  void debugDrawBelow (QPainter *painter) const;
  void debugDrawAbove (QPainter *painter) const;
#else
  void debugDrawBelow (QPainter*) const {}
  void debugDrawAbove (QPainter*) const {}
#endif

  void printPhenotypePdf (const QString &filename) const;
  void printPhenotypePng (const QString &filename) const;
};

struct CID {
  const Critter &c;
  const QString prefix;
  CID (const Critter *c, const QString &p = "VC") : CID(*c, p) {}
  CID (const Critter &c, const QString &p = "VC") : c(c), prefix(p) {}
  friend QDebug operator<<(QDebug dbg, const CID &cid);
};

} // end of namespace visu

#endif // VISU_CRITTER_H
