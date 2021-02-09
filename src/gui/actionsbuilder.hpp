#ifndef ACTIONSBUILDER_HPP
#define ACTIONSBUILDER_HPP

#include <QAction>

namespace gui {

namespace helpers {

struct BoolAction : public QAction {
  bool &value;
  BoolAction (const QString &name, QWidget *parent, bool &v)
    : QAction(name, parent), value(v) {

    setCheckable(true);
    setChecked(value);

    connect(this, &QAction::triggered, this, &BoolAction::toggle);
  }

  void toggle (void) {
    value = !value;
  }
};

template <typename T>
struct EnumAction : public QAction {
  T &value;
  QString details;

  using Incrementer = std::function<T(T)>;
  const Incrementer incrementer;

  using Formatter = std::function<std::ostream&(std::ostream&, const T&)>;
  const Formatter formatter;

  EnumAction (const QString &name, const QString &details, QWidget *parent,
              T &v, Incrementer i, Formatter f)
    : QAction(name, parent),
      value(v), details(details), incrementer(i), formatter(f) {

    connect(this, &QAction::triggered, this, &EnumAction::next);
    updateStatus();
  }

  void next (void) {
    value = incrementer(value);
    updateStatus();
  }

  void updateStatus (void) {
    std::ostringstream oss;
    oss << details.toStdString()
        << "; Current status for " << text().toStdString() << ": ";
    formatter(oss, value);
    oss << ")";
    setStatusTip(QString::fromStdString(oss.str()));
  }
};

} // end of namespace helpers

} // end of namespace gui

#endif // ACTIONSBUILDER_HPP
