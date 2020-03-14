#ifndef B2DEBUGDRAWER_H
#define B2DEBUGDRAWER_H

#include <memory>

#include "box2d/b2_draw.h"

#include <QGraphicsItem>

namespace visu {

QColor toQt (const b2Color &c);

struct DebugDrawer : public b2Draw, public QGraphicsItem {

  struct b2DD {
    QColor color;
    bool filled;

    b2DD (const b2Color &c, bool f) : color(toQt(c)), filled(f) {}

    virtual ~b2DD (void) = default;

    void draw (QPainter *p) const;

    void prePaint (QPainter *p) const;

    void postPaint (QPainter *p) const;

    virtual void doPaint (QPainter *p) const = 0;
  };

  std::vector<std::unique_ptr<b2DD>> _draws;

  virtual ~DebugDrawer (void);

  void DrawPolygon(const b2Vec2* vertices, int32 vertexCount,
                   const b2Color& color) override;

  void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount,
                        const b2Color& color) override;

  void DrawCircle(const b2Vec2& center, float radius,
                  const b2Color& color) override;

  void DrawSolidCircle(const b2Vec2& center, float radius,
                       const b2Vec2& axis, const b2Color& color) override;

  void DrawSegment(const b2Vec2& p1, const b2Vec2& p2,
                   const b2Color& color) override;

  void DrawTransform(const b2Transform& /*xf*/) override;

  void DrawPoint(const b2Vec2& p, float size, const b2Color& color) override;

  void clear (void) {
    _draws.clear();
  }

  auto size (void) const {
    return _draws.size();
  }

  QRectF boundingRect(void) const override;
  void paint (QPainter *p,
              const QStyleOptionGraphicsItem*, QWidget*) override;
};

} // end of namespace visu

#endif // B2DEBUGDRAWER_H
