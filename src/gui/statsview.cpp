#include "statsview.h"
#include <QFormLayout>

namespace gui {

StatsView::StatsView (void) {
  setObjectName("splinoids::gui::statsview");
}

QFrame* line (void) {
  QFrame *l = new QFrame;
  l->setFrameShape(QFrame::HLine);
  l->setFrameShadow(QFrame::Sunken);
  return l;
}

void StatsView::setupFields (const QStringList &l) {
  QFormLayout *layout = new QFormLayout;

  auto addRow = [layout, this] (QString field) {
    if (field.startsWith("-- ")) {
      field = field.replace(QRegExp("[- ]"), "");
      QHBoxLayout *l = new QHBoxLayout;
      l->addWidget(line());
      l->addWidget(new QLabel (field));
      l->addWidget(line());
      layout->addRow(l);

    } else {
      auto prettyfield = field;
      prettyfield.replace(QRegExp("\\[[^]]*\\] "), "");
      QLabel *label = new QLabel;
      details[field] = label;
      layout->addRow(prettyfield, label);
    }
  };

  QLabel *header = new QLabel ("Details");
  header->setStyleSheet("font-size: 14px; font-style: italic");
  layout->addRow(header);

  for (const QString &s: l) addRow(s);

  setLayout(layout);
}

} // end of namespace gui
