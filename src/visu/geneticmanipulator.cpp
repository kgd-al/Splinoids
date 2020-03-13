#include <QHBoxLayout>
#include <QSlider>
#include <QLabel>
#include <QGroupBox>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>

#include "geneticmanipulator.h"

#include "config.h"
#include "../genotype/critter.h"

#include <QDebug>

namespace visu {

using CGenome = genotype::Critter;

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
    if (object)
      r = object->maximalBoundingRect().united(object->boundingRect());
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
        painter->drawRect(object->maximalBoundingRect());
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

void MiniViewer::resizeEvent(QResizeEvent */*e*/) {
//  qDebug() << sceneRect();
  fitInView(sceneRect(), Qt::KeepAspectRatio);
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
    : QSlider (Qt::Horizontal), i(i), j(j), min(min), max(max) {

    setMinimum(scale(min));
    setMaximum(scale(max));
  }

  void noValue (void) {
    setValue (SCALE/2);
    setEnabled(false);
  }

  void setGeneValue (T v) {
    QSlider::setValue(scale(v));
    setEnabled(true);
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
};

// =============================================================================

GeneticManipulator::GeneticManipulator(QWidget *parent)
  : QDialog(parent) {
  _proxy = new CritterProxy;

  const auto &sBounds = genotype::Spline::config_t::dataBounds();

  QHBoxLayout *rootLayout = new QHBoxLayout;
    _viewer = new MiniViewer (this, _proxy);
    QGridLayout *fieldsLayout = new QGridLayout;
    for (uint i=0; i<S_v; i++) {
      for (uint j=0; j<SD_v; j++) {
        GeneSlider *s;
        fieldsLayout->addWidget(s = _sSliders[i][j]
                                  = new GeneSlider(sBounds, i, j), j, i);
        connect(s, &GeneSlider::valueChanged,
                this, &GeneticManipulator::updateGeneValue);
      }

      for (uint j=0; j<2; j++) {
        GeneSlider *s;
        fieldsLayout->addWidget(s = _dSliders[i][j]
                                  = new GeneSlider(0, 1, i, j+SD_v), j+SD_v+1, i);
        connect(s, &GeneSlider::valueChanged,
                this, &GeneticManipulator::updateGeneValue);
      }
    }

    for (uint j=0; j<SD_v+3; j++)
      fieldsLayout->addWidget(new QLabel(names[j]), j, CGenome::SPLINES_COUNT);

    QFrame* line1 = new QFrame();
    line1->setFrameShape(QFrame::HLine);
    line1->setFrameShadow(QFrame::Sunken);
    fieldsLayout->addWidget(line1, SD_v, 0, 1, -1);

    QFrame* line2 = new QFrame();
    line2->setFrameShape(QFrame::HLine);
    line2->setFrameShadow(QFrame::Sunken);
    fieldsLayout->addWidget(line2, SD_v+3, 0, 1, -1);

    QGroupBox *sbox = new QGroupBox;
    sbox->setFlat(true);
    sbox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    QHBoxLayout *sboxLayout = new QHBoxLayout;
    sboxLayout->addWidget(_sex[0] = new QRadioButton("Female"));
    sboxLayout->addWidget(_sex[1] = new QRadioButton("Male"));
    fieldsLayout->addWidget(sbox, SD_v+4, 1, 1, 2);
    sbox->setLayout(sboxLayout);
    for (QRadioButton *b: _sex)
      connect(b, &QRadioButton::clicked, this, &GeneticManipulator::sexChanged);

    QFrame *filler = new QFrame();
//    filler->setStyleSheet("background-color:red");
    filler->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    fieldsLayout->addWidget(filler, SD_v+5, 0, 1, SD_v);

    rootLayout->addWidget(_viewer);
    rootLayout->addLayout(fieldsLayout);
  setLayout(rootLayout);

  _updating = false;
  setSubject(nullptr);
}

void GeneticManipulator::updateWindowName (void) {
  QString name = "Genetic Manipulator";
  if (_subject)
    name += ": " + QString::number(uint(genome().id()));
  setWindowTitle(name);
}

void GeneticManipulator::setSubject(Critter *s) {
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
      for (QRadioButton *b: _sex) b->setEnabled(true);
      _sex[int(g.sex())]->setChecked(true);
    }

  } else {
    for (uint i=0; i<S_v; i++) {
      for (uint j=0; j<SD_v; j++) _sSliders[i][j]->noValue();
      for (uint j=0; j<2; j++)    _dSliders[i][j]->noValue();
      for (QRadioButton *b: _sex) b->setEnabled(false);
    }
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

void GeneticManipulator::sexChanged(void) {
  if (_updating)  return;

  QRadioButton *b = dynamic_cast<QRadioButton*>(sender());
  if (!b) throw std::logic_error("Sex updated outside QRadioButton!");

  auto &s = _subject->object().genotype().cdata.sex;
  if (b == _sex[0])       s = CGenome::Sex::FEMALE;
  else if (b == _sex[1])  s = CGenome::Sex::MALE;
  else
    throw std::logic_error("Sex update value from unknown QRadioButton");

  updateSubject();
}

void GeneticManipulator::keyReleaseEvent(QKeyEvent *e) {
  qDebug() << __PRETTY_FUNCTION__ << ": " << e;
  emit keyReleased(e);
  _proxy->update();
}


} // end of namespace visu
