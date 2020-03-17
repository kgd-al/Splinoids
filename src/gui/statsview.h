#ifndef STATSVIEW_H
#define STATSVIEW_H

#include <iostream>

#include "../../qt-collapsible-section/Section.h"

#include <QString>
#include <QLabel>
#include <QMap>

#include <QFormLayout>

namespace gui {

class StatsView : public Section {
  QMap<QString, QLabel*> details;

public:
  StatsView(void);

  template <typename T>
  void update (const QString &field, T value) {
    QLabel *l = details.value(field);
    if (l)  l->setText(QString::number(value));
    else
      std::cerr << field.toStdString() << " is not a stats field!" << std::endl;
  }
};

} // end of namespace gui

#endif // STATSVIEW_H
