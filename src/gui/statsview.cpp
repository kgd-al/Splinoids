#include "statsview.h"

namespace gui {

StatsView::StatsView(void) : Section("Details") {
  QFormLayout *layout = new QFormLayout;

  auto addRow = [layout, this] (const QString &field) {
    QLabel *label = new QLabel;
    details[field] = label;
    layout->addRow(field, label);
  };

  for (const QString &s: { "Critters", "Plants" })
    addRow(s);

  setContentLayout(*layout);
}


} // end of namespace gui
