#include <QHBoxLayout>
#include <QFormLayout>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>

#include "geneticmanipulator.h"

#include "../visu/config.h"
#include "../genotype/critter.h"

#include <QDebug>

namespace gui {

using CGenome = genotype::Critter;
static constexpr auto NO_FIRSTNAME = "Unknown";
static constexpr auto NO_LASTNAME = "Doe";

class CritterProxy : public QGraphicsItem {
  visu::Critter *object;

public:
  CritterProxy (void) : object(nullptr) {}

  void setObject(visu::Critter *o) {
    prepareGeometryChange();
    object = o;
  }

  void objectUpdated (void) {
    prepareGeometryChange();
    update();
  }

  QRectF boundingRect (void) const {
    QRectF r;
    if (object) {
//      QTransform t;
//      t.rotate(90);
//      r = t.mapRect(object->minimalBoundingRect());
      r = object->minimalBoundingRect();
    }
//    qDebug() << r;
    return r;
  }

  void paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*) {
    if (object) {
      painter->save();
        painter->rotate(90);
        object->doPaint(painter);

        QPen pen = painter->pen();
        pen.setStyle(Qt::DotLine);
        pen.setColor(Qt::red);
        pen.setWidthF(0.);
        painter->setPen(pen);
        painter->drawRect(object->critterBoundingRect());

        pen.setStyle(Qt::DashLine);
        pen.setColor(Qt::blue);
        pen.setWidthF(0.);
        painter->setPen(pen);
        painter->drawRect(boundingRect());
      painter->restore();
    }
  }

  void mousePressEvent(QGraphicsSceneMouseEvent */*e*/) override {
//    QGraphicsItem::mousePressEvent(e);
//    qDebug() << "Pressed";
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override {
    if (Qt::ShiftModifier == e->modifiers())
      object->save();
  }
};

// =============================================================================

MiniViewer::MiniViewer (QWidget *parent, CritterProxy *proxy)
  : QGraphicsView(parent) {

  setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
  setBackgroundBrush(Qt::white);
  setScene(new QGraphicsScene);
  scene()->addItem(proxy);

  auto Z = config::Visualisation::viewZoom();
  scale(Z, -Z);
}

MiniViewer::~MiniViewer (void) {
  delete scene();
}

void MiniViewer::resizeEvent(QResizeEvent */*e*/) {
//  qDebug() << sceneRect();
  fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
}

// =============================================================================

static constexpr const char *names [] {
  "SA",
  "EA", "EL",
  "DX0", "DY0", "DX1", "DY1",
  "W0", "W1", "W2",
  "",
  "XX", "XY"
};

using SData = GeneticManipulator::SData;
struct GeneSlider : public QSlider {
  static constexpr int SCALE = 100;
  using T = SData::value_type;

  template <typename B>
  GeneSlider (const B &b, uint i, uint j)
    : GeneSlider(b.min[j], b.max[j], i, j) {}

  template <typename T_>
  GeneSlider (T_ min, T_ max, uint i, uint j)
    : QSlider (Qt::Horizontal), i(i), j(j), min(min), max(max),
      valueset(false), readonly(false) {

    setMinimum(scale(min));
    setMaximum(scale(max));
  }

  void setReadOnly (bool r) {
    readonly = r;
    setEnabled();
  }

  void noValue (void) {
    setValue (SCALE/2);
    valueset = false;
    setEnabled();
  }

  void setGeneValue (T v) {
    QSlider::setValue(scale(v));
    valueset = true;
    setEnabled();
  }

  void setEnabled(void) {
    QSlider::setEnabled(valueset && !readonly);
  }

  T geneValue (void) const {
    return invertScale(value());
  }

  int scale (T v) const {
    return SCALE * (v - min) / (max - min);
  }

  T invertScale (int v) const {
    return (max - min) * float(v) / float(SCALE) + min;
  }

  auto splineIndex (void) const {
    return i;
  }

  auto splineDataIndex (void) const {
    return j;
  }

  void paintEvent(QPaintEvent *e) {
    QSlider::paintEvent(e);
    qreal x = width() * float(value() - minimum()) / (maximum() - minimum());

    QString text = QString::number(geneValue(), 'f', 2);
    QPainter painter (this);

    QRectF bounds;
    painter.drawText(QRectF(), Qt::AlignCenter, text, &bounds);

    x = std::max(x, .5f*bounds.width());
    x = std::min(x, width() - .5f*bounds.width());
    bounds.translate(x, .5*bounds.height());
//    painter.fillRect(bounds, Qt::red);
    painter.drawText(bounds, Qt::AlignCenter, text);
//    qDebug() << "painter.drawText(" << bounds << "," << text << ") with h ="
//             << height() << "and bh =" << bounds.height();
  }

private:
  const uint i,j;
  const T min, max;
  bool valueset, readonly;
};

// =============================================================================

struct GeneColorPicker : public QLabel {
  using Color = genotype::Color;

  template <typename B>
  GeneColorPicker (const B &b, uint i, uint j)
    : i(i), j(j), min(b.min), max(b.max), readonly(false) {
    setAutoFillBackground(true);
  }

  void setReadOnly (bool r) {
    readonly = r;
    if (r)  setEnabled(false);
  }

  void noValue (void) {
    setValue ({.5,.5,.5});
    setEnabled(false);
  }

  void setGeneValue (const Color &c) {
    setValue(c);
    setEnabled(true && !readonly);
  }

  void mouseReleaseEvent(QMouseEvent*) {
    qDebug() << "Want to change color...";
  }

private:
  const uint i, j;
  const Color::value_type min, max;
  QColor color;
  bool readonly;

  void setValue (const Color &c) {
    color = toQt(c);
    QPalette p = palette();
    p.setColor(QPalette::Window, color);
    setPalette(p);
    update();
  }
};

// =============================================================================

QFrame* line (void) {
  QFrame* l = new QFrame();
  l->setFrameShape(QFrame::HLine);
  l->setFrameShadow(QFrame::Sunken);
  return l;
}

QFrame* filler (bool vertical = false) {
  QFrame *f = new QFrame();
  f->setSizePolicy(QSizePolicy::MinimumExpanding,
                   vertical ? QSizePolicy::MinimumExpanding
                            : QSizePolicy::Fixed);

  f->setStyleSheet("background-color:red");

  return f;
}

GeneticManipulator::GeneticManipulator(QWidget *parent)
  : QDialog(parent), _subject(nullptr), _readonly(true) {
  _proxy = new CritterProxy;

  buildViewer();
  buildStatsLayout();
  buildGenesLayout();

  _editButton = new QPushButton ("");
  _hideButton = new QPushButton ("Close");

  connect(_editButton, &QPushButton::clicked,
          this, &GeneticManipulator::toggleReadOnly);

  connect(_hideButton, &QPushButton::clicked,
          this, &GeneticManipulator::hide);

  QVBoxLayout *rootLayout = new QVBoxLayout;
    _contentsLayout = new QVBoxLayout;

    _innerLayout = new QHBoxLayout;
      _innerLayout->addWidget(_viewer);
      _innerLayout->addLayout(_statsLayout);
    _contentsLayout->addLayout(_innerLayout);
    _contentsLayout->addLayout(_genesLayout);

    QHBoxLayout *buttonsLayout = new QHBoxLayout;
    buttonsLayout->addWidget(filler(false));
    buttonsLayout->addWidget(_editButton, 0, Qt::AlignCenter);
    buttonsLayout->addWidget(_hideButton, 0, Qt::AlignCenter);
    buttonsLayout->addWidget(filler(false));
  rootLayout->addLayout(_contentsLayout);
  rootLayout->addLayout(buttonsLayout);
  setLayout(rootLayout);

  setReadOnly(_readonly);

  _updating = false;
  setSubject(nullptr);
}

void GeneticManipulator::buildViewer(void) {
  _viewer = new MiniViewer (this, _proxy);
}

void GeneticManipulator::buildStatsLayout(void) {
  QFormLayout *sl;
  _statsLayout = sl = new QFormLayout;

  sl->addRow(_dataWidgets.firstname = new QLabel,
             _dataWidgets.lastname = new QLabel);

  QString headerStyleSheet = " font-size: 18px; font-weight: bold ";
  _dataWidgets.firstname->setStyleSheet(headerStyleSheet);
  _dataWidgets.lastname->setStyleSheet(headerStyleSheet);

  QComboBox *cbSex = new QComboBox;
  cbSex->insertItem(simu::Critter::Sex::FEMALE, "female");
  cbSex->insertItem(  simu::Critter::Sex::MALE, "male");
  cbSex->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  connect(cbSex, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &GeneticManipulator::sexChanged);
  _dataWidgets.sex = cbSex;
  sl->addRow(cbSex);

  sl->addRow("Mass", _dataWidgets.mass = new QLabel);

  sl->addRow(filler(true));
}

void GeneticManipulator::buildGenesLayout(void) {
  const auto &sBounds = genotype::Spline::config_t::dataBounds();
  const auto &cBounds = genotype::Critter::config_t::color_bounds();

  QVBoxLayout *gl;
  _genesLayout = gl = new QVBoxLayout;

  QGridLayout *splinesLayout = new QGridLayout;
  gl->addWidget(line());
  gl->addWidget(new QLabel("Splines"));
  gl->addLayout(splinesLayout);
  for (uint i=0; i<S_v; i++) {
    for (uint j=0; j<SD_v; j++) {
      GeneSlider *s;
      splinesLayout->addWidget(s = _sSliders[i][j]
                                = new GeneSlider(sBounds, i, j), j, i);
      connect(s, &GeneSlider::valueChanged,
              this, &GeneticManipulator::updateGeneValue);
    }

    for (uint j=0; j<2; j++) {
      GeneSlider *s;
      splinesLayout->addWidget(s = _dSliders[i][j]
                                = new GeneSlider(0, 1, i, j+SD_v), j+SD_v+1, i);
      connect(s, &GeneSlider::valueChanged,
              this, &GeneticManipulator::updateGeneValue);
    }
  }

  for (uint j=0; j<SD_v+3; j++)
    splinesLayout->addWidget(new QLabel(names[j]), j, CGenome::SPLINES_COUNT);

  splinesLayout->addWidget(line(), SD_v, 0, 1, -1);

  QGridLayout *colorsLayout = new QGridLayout;
  gl->addWidget(line());
  gl->addWidget(new QLabel("Colors"));
  gl->addLayout(colorsLayout);

  // Splines colors
  for (uint i=0; i<S_v; i++) {
    for (uint j=0; j<2; j++) {
      GeneColorPicker *p;
      colorsLayout->addWidget(p = _sPickers[i][j]
                                = new GeneColorPicker (cBounds, i, j), 2*j, i);
    }
  }

  // Body colors
  for (uint j=0; j<2; j++) {
    GeneColorPicker *p;
    colorsLayout->addWidget(p = _bPickers[j]
                              = new GeneColorPicker (cBounds, 0, j+S_v),
                            2*j+1, 0, 1, S_v);
  }

  gl->addWidget(filler(true));
}

void GeneticManipulator::updateWindowName (void) {
  QString name = _readonly ? "Splinoid Sheet" : "Genetic Manipulator";
  if (_subject)
    name += ": " + QString::number(uint(genome().id()));
  setWindowTitle(name);
}

void GeneticManipulator::setReadOnly(bool r) {
  for (auto &sd: _sSliders) for (auto *s: sd) s->setReadOnly(r);
  for (auto &da: _dSliders) for (auto *s: da) s->setReadOnly(r);
  for (auto &sc: _sPickers) for (auto *p: sc) p->setReadOnly(r);
  for (auto *p: _bPickers)  p->setReadOnly(r);
  _dataWidgets.sex->setEnabled(!r);

  // Set appropriate layout directions
  _contentsLayout->setDirection(
    _readonly ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
  _innerLayout->setDirection(
    _readonly ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);

  _editButton->setText(r ? "Edit" : "Ok");

  updateWindowName();
}

void GeneticManipulator::setSubject(visu::Critter *s) {
  _updating = true;

  _subject = s;

  _proxy->setObject(s);

  if (_subject) {
    const auto &g = s->object().genotype();
    for (uint i=0; i<S_v; i++) {
      for (uint j=0; j<SD_v; j++)
        _sSliders[i][j]->setGeneValue(g.splines[i].data[j]);
      for (uint j=0; j<2; j++)
        _dSliders[i][j]->setGeneValue(g.dimorphism[4*j+i]);

      for (uint j=0; j<2; j++)
        _sPickers[i][j]->setGeneValue(g.colors[j*(S_v+1)+i+1]);
    }

    for (uint j=0; j<2; j++)
      _bPickers[j]->setGeneValue(g.colors[j*(S_v+1)]);

    _dataWidgets.sex->setEnabled(true && !_readonly);
    _dataWidgets.sex->setCurrentIndex(
      int(_subject->object().genotype().cdata.sex));

    _dataWidgets.firstname->setText("0x" + QString::number(long(g.id()), 16));

    static constexpr auto NO_SPECIES = phylogeny::SID::INVALID;
    auto species = g.species();
    _dataWidgets.lastname->setText(
      species == NO_SPECIES ? NO_LASTNAME
                            : "0x" + QString::number(long(species), 16));

    _dataWidgets.mass->setText(QString::number(_subject->object().mass()));

  } else {
    for (uint i=0; i<S_v; i++) {
      for (uint j=0; j<SD_v; j++) _sSliders[i][j]->noValue();
      for (uint j=0; j<2; j++)    _dSliders[i][j]->noValue();

      for (uint j=0; j<2; j++)    _sPickers[i][j]->noValue();
    }

    for (uint j=0; j<2; j++)  _bPickers[j]->noValue();

    _dataWidgets.sex->setEnabled(false);

    _dataWidgets.firstname->setText("Nobody");
    _dataWidgets.lastname->setText("Doe");

    _dataWidgets.mass->setText("0");
  }

  updateWindowName();

  _updating = false;
}

void GeneticManipulator::updateSubject(void) {
  _subject->object().updateShape();
  _subject->updateShape();
  _subject->update();
  _proxy->objectUpdated();
}

void GeneticManipulator::updateGeneValue(void) {
  if (_updating)  return;

  GeneSlider *s = dynamic_cast<GeneSlider*>(sender());
  if (!s) throw std::logic_error("Gene value updated outside GeneSlider!");

  auto &g = _subject->object().genotype();
  auto i = s->splineIndex(), j = s->splineDataIndex();
  if (j < SD_v) // spline data
    g.splines[i].data[j] = s->geneValue();

  else  // dimorphism data
    g.dimorphism[4*(j-SD_v)+i] = s->geneValue();

  updateSubject();
}

void GeneticManipulator::sexChanged(int i) {
  if (_updating)  return;

//  QRadioButton *b = dynamic_cast<QRadioButton*>(sender());
//  if (!b) throw std::logic_error("Sex updated outside QRadioButton!");

//  auto &s = _subject->object().genotype().cdata.sex;
//  if (b == _sex[0])       s = CGenome::Sex::FEMALE;
//  else if (b == _sex[1])  s = CGenome::Sex::MALE;
//  else
//    throw std::logic_error("Sex update value from unknown QRadioButton");

  _subject->object().genotype().cdata.sex = Sex(i);

  updateSubject();
}

void GeneticManipulator::keyReleaseEvent(QKeyEvent *e) {
//  qDebug() << __PRETTY_FUNCTION__ << ": " << e;
  emit keyReleased(e);
  _proxy->update();
}


} // end of namespace gui
