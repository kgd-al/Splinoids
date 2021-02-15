#ifndef STATSVIEW_H
#define STATSVIEW_H

#include <iostream>

#include <QString>
#include <QLabel>
#include <QMap>

namespace gui {

class StatsView : public QWidget {
  QMap<QString, QLabel*> details;

public:
  StatsView(void);

  void setupFields (const QStringList &l);

  template <typename T>
  void update (const QString &field, T value, int precision = 2) {
    QLabel *l = details.value(field);
    if (l)  l->setText(QString::number(value, 'f', precision));
    else
      std::cerr << field.toStdString() << " is not a stats field!" << std::endl;
  }
};

} // end of namespace gui

#endif // STATSVIEW_H
