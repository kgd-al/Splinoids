#ifndef VISU_FOODLET_H
#define VISU_FOODLET_H

#include <QGraphicsItem>

#include "../simu/foodlet.h"

namespace visu {

class Foodlet : public QGraphicsItem {
private:
  simu::Foodlet &_foodlet;
  QPainterPath _body;

public:
  QString tag;

  Foodlet(simu::Foodlet &foodlet);

  const auto& object (void) const {
    return _foodlet;
  }

  auto& object (void) {
    return _foodlet;
  }

  QRectF boundingRect (void) const;

  void paint(QPainter *painter,
             const QStyleOptionGraphicsItem*, QWidget *) override;
};

} // end of namespace visu

#endif // VISU_FOODLET_H
