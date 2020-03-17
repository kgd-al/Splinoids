#ifndef VISU_PLANT_H
#define VISU_PLANT_H

#include <QGraphicsItem>

#include "../simu/plant.h"

namespace visu {

class Plant : public QGraphicsItem {
private:
  simu::Plant &_plant;
  QPainterPath _body;

public:
  Plant(simu::Plant &plant);

  const auto& object (void) const {
    return _plant;
  }

  auto& object (void) {
    return _plant;
  }

  void update (void);

  QRectF boundingRect (void) const;

  void paint(QPainter *painter,
             const QStyleOptionGraphicsItem*, QWidget *) override;
};

} // end of namespace visu

#endif // VISU_PLANT_H
