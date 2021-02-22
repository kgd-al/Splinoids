#include <QApplication>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QSlider>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QToolButton>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QFileDialog>
#include <QColorDialog>
#include <QDesktopWidget>

#include "geneticmanipulator.h"

#include "../visu/config.h"
#include "../genotype/critter.h"

#include <QDebug>

namespace gui {

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
    if (object) {
      QRectF ombr = object->minimalBoundingRect();
      r.setRect(ombr.y(), ombr.x(), ombr.height(), ombr.width());
    }
    return r;
  }

  void paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*) {
    if (object) {
      painter->save();
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

        painter->rotate(90);
        object->doPaint(painter);
      painter->restore();
    }
  }

//  void mousePressEvent(QGraphicsSceneMouseEvent */*e*/) override {
////    QGraphicsItem::mousePressEvent(e);
////    qDebug() << "Pressed";
//  }

//  void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override {
//    if (Qt::ShiftModifier == e->modifiers())
//      objectprintve();
//  }
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
  fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
}

// =============================================================================

struct LabeledSlider : public QSlider {
  static constexpr int SCALE = 100;

  LabeledSlider (double min, double max)
    : QSlider (Qt::Horizontal), min(min), max(max) {
    setRange(min, max);
    setEnabled(false);
  }

  int scale (double v) const {
    return SCALE * (v - min) / (max - min);
  }

  void setRange (double min, double max) {
    setMinimum(scale(min));
    setMaximum(scale(max));
  }

  void setValue (double v) {
    QSlider::setValue(scale(v));
  }

  double invertScale (int v) const {
    return (max - min) * float(v) / float(SCALE) + min;
  }

  double value (void) const {
    return invertScale(scaledValue());
  }

  void noValue (void) {
    setValue (SCALE/2);
  }

  int scaledValue (void) const {
    return QSlider::value();
  }

  void paintEvent(QPaintEvent *e) {
    QSlider::paintEvent(e);
    qreal x = width() * float(scaledValue() - minimum())
            / (maximum() - minimum());

    QString text = QString::number(value(), 'f', 2);
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
  const double min, max;

  using QSlider::value;
  using QSlider::setValue;
  using QSlider::setRange;
};

// =============================================================================

static constexpr const char *snames [] {
  "SA",
  "EA", "EL",
  "DX0", "DY0", "DX1", "DY1",
  "W0", "W1", "W2",
  "",
  "XX", "XY"
};

using SData = GeneticManipulator::SData;
struct GeneSlider : public LabeledSlider {
  using T = SData::value_type;

  template <typename B>
  GeneSlider (const B &b, uint i, uint j)
    : GeneSlider(b.min[j], b.max[j], i, j) {}

  GeneSlider (double min, double max, uint i, uint j)
    : LabeledSlider (min, max), i(i), j(j),
      valueset(false), readonly(false) {}

  void setReadOnly (bool r) {
    readonly = r;
    setEnabled();
  }

  void noValue (void) {
    LabeledSlider::noValue();
    valueset = false;
    setEnabled();
  }

  void setGeneValue (T v) {
    LabeledSlider::setValue(v);
    valueset = true;
    setEnabled();
  }

  void setEnabled(void) {
    LabeledSlider::setEnabled(valueset && !readonly);
  }

  T geneValue (void) const {
    return value();
  }

  auto splineIndex (void) const {
    return i;
  }

  auto splineDataIndex (void) const {
    return j;
  }

private:
  const uint i,j;
  bool valueset, readonly;
};

// =============================================================================

static constexpr const char *cnames [] {
  "SXX", "BXX", "SXY", "BXY"
};


struct ColorLabel : public QLabel {
  using Color = genotype::Color;

  ColorLabel (void) {
    setAutoFillBackground(true);
    setFrameStyle(QFrame::Box);
    setEnabled(false);
  }

  void noValue (void) {
    setColorValue (Qt::black);
  }

  void setValue (const Color &c) {
    setColorValue(c);
  }

  void setValue(float v) {
    setColorValue(QColor::fromHsvF(0, 0, v));
  }

protected:
  QColor color;

  void setColorValue (const Color &c) {
    setColorValue(toQt(c));
  }

  void setColorValue (const QColor &c) {
    color = c;
    QPalette p = palette();
    p.setColor(QPalette::Window, c);
    setPalette(p);
    update();
  }
};

class ColorLabels : public QWidget {
  QVector<ColorLabel*> _labels;

public:
  ColorLabels (int n, bool split) {
    QHBoxLayout *l = new QHBoxLayout;
    l->setSpacing(0);

    _labels.resize(n+1);
    for (int i=0; i<n+1; i++) {
      ColorLabel *cl = _labels[i] = new ColorLabel;
      if (i == 0) {
        cl->setAlignment(Qt::AlignCenter);
        cl->setStyleSheet("border: 1px solid gray");

      } else {
        cl->noValue();
        cl->hide();
      }

      l->addWidget(cl);

      if (split && i == n/2) l->addSpacing(10);
    }

    l->setContentsMargins(0, 0, 0, 0);
    setContentsMargins(0, 0, 0, 0);
    setLayout(l);
  }

  void setEnabled (bool enabled) {
    _labels[0]->setVisible(!enabled);
    for (int i=1; i<_labels.size(); i++) _labels[i]->setVisible(enabled);
  }

  ColorLabel* label (int i) {
    return _labels[i+1];
  }

  uint size (void) const {
    return _labels.size() - 1;
  }
};

struct GeneColorPicker : public ColorLabel {
  template <typename B>
  GeneColorPicker (const B &b, uint i, uint j)
    : i(i), j(j), min(b.min), max(b.max), readonly(false) {}

  void setReadOnly (bool r) {
    readonly = r;
    setEnabled(!readonly);
  }

  void noValue (void) {
    setColorValue (Qt::gray);
    setEnabled(false);
  }

  void setGeneValue (const Color &c) {
    setColorValue(c);
    setEnabled(true && !readonly);
  }

  void mouseReleaseEvent(QMouseEvent*) {
    auto selected = QColorDialog::getColor(color, this);
    qDebug() << "requested color change from" << color << "to"
             << selected;
  }

private:
  const uint i, j;
  const Color::value_type min, max;
  bool readonly;
};

// =============================================================================

struct PrettyBar : public QProgressBar {
  static constexpr int MAX = 1000, SCALE = 1000;
  enum Type {
    BODY_HEALTH, BODY_ENERGY, SPLINE_HEALTH
  };
  Type type;
  QColor chunkColor;

  float max = -1, scale = 0;

  PrettyBar (Type t) : QProgressBar() {
    type = t;

    QColor c;
    if (t == BODY_HEALTH)           c = QColor(Qt::red);
    else if (t == BODY_ENERGY)      c = QColor(Qt::blue);
    else if (t == SPLINE_HEALTH) {  c = QColor(Qt::darkRed);
      setOrientation(Qt::Vertical);
    }
    chunkColor = c;
    setStyle(chunkColor);

    if (type != SPLINE_HEALTH)
          prettyFormat();
    else  setTextVisible(false);
  }

  void noValue (void) {
    setMaximum(0, false);
    setValue(0);
    setFormat("");
  }

  void setMaximum (float m, bool destroyed) {
    if (m == 0) {
      max = -1;
      setStyle(palette().base().color());
      QProgressBar::setRange(-1, -1);

    } else {
      max = m;
      setStyle(chunkColor);
      QProgressBar::setRange(0, MAX);
    }

    if (type == SPLINE_HEALTH) {
      if (destroyed)
        setFormat("Destroyed");
      else if (m == 0)
        setFormat("Inactive");
      else
        setFormat("");
    } else
      resetFormat();
  }

  void setValue (float v) {
    QProgressBar::setValue(MAX * v / max);
    prettyFormat();
  }

  void setStyle (const QColor &cc) {
    QString styleSheet =
      "QProgressBar {"
        " border: 0px;"
        " border-radius: 5px;";
    if (type != SPLINE_HEALTH)
      styleSheet += " text-align: center;";
    styleSheet +=
      " } "
      "QProgressBar::chunk { background-color: #"
      + QString::number(cc.rgb(), 16)
      + "; border-radius: 5px }";
    setStyleSheet(styleSheet);
  }

  void paintEvent(QPaintEvent *e) {
    QProgressBar::paintEvent(e);

    const QString &f = format();
    if (!isTextVisible() && !format().isEmpty()) {
      QRect cr = contentsRect(), br;
      QPainter painter (this);
      QPen pen = painter.pen();
      painter.translate(cr.center());
      painter.rotate(-90);
      painter.translate(-cr.center());
//      painter.setPen(Qt::NoPen);
      painter.drawText(cr, Qt::AlignCenter, f, &br);
      if (f == "Inactive")  pen.setColor(Qt::gray);
      painter.setPen(pen);
//      painter.fillRect(br, Qt::red);
      painter.drawText(br, Qt::AlignCenter, f);
//      qDebug() << "DrawText(" << text() << cr << ") >> " << br;
      if (f == "Destroyed") {
        painter.drawLine(br.topLeft(), br.bottomRight());
        painter.drawLine(br.bottomLeft(), br.topRight());
      }
    }
  }

private:
  void prettyFormat (void) {
    setFormat(QString::number(SCALE * floatValue(), 'f', 0)
              + " / "
              + QString::number(SCALE * max, 'f', 0));
  }

  float floatValue (void) const {
    if (max == -1)  return NAN;
    return max * value() / MAX;
  }
};

// =============================================================================

QWidget* line (const QString &text = QString()) {
  if (text.isEmpty()) {
    QFrame *f = new QFrame;
    f->setFrameShape(QFrame::HLine);
    f->setFrameShadow(QFrame::Sunken);
    return f;

  } else {
    QWidget *holder = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    holder->setLayout(layout);
    layout->addWidget(line(), 1);
    layout->addWidget(new QLabel(text), 0);
    layout->addWidget(line(), 1);
    return holder;
  }
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
  setObjectName("splinoids::gui::geneticmanipulator");
//  setStyleSheet("font-size: 7pt");

  _proxy = new CritterProxy;

  buildViewer();
  buildStatsLayout();
  buildGenesLayout();

  buildNeuralPanel();

  _brainButton = new QPushButton;
  _brainButton->setIcon(QIcon::fromTheme("help-about"));
  connect(_brainButton, &QPushButton::clicked,
          this, qOverload<>(&GeneticManipulator::saveSubjectBrain));

  _saveButton = new QPushButton;
  _saveButton->setIcon(QIcon::fromTheme("document-save"));  
  connect(_saveButton, &QPushButton::clicked,
          this, qOverload<>(&GeneticManipulator::saveSubjectGenotype));

  _printButton = new QPushButton;
  _printButton->setIcon(QIcon::fromTheme("document-print"));
  connect(_printButton, &QPushButton::clicked,
          this, qOverload<>(&GeneticManipulator::printSubjectPhenotype));

  _editButton = new QPushButton;
  connect(_editButton, &QPushButton::clicked,
          this, &GeneticManipulator::toggleReadOnly);
  _editButton->setShortcut(Qt::Key_E);

  _hideButton = new QPushButton ("Close");
  connect(_hideButton, &QPushButton::clicked,
          this, &GeneticManipulator::hide);

  QVBoxLayout *rootLayout = new QVBoxLayout;
    _contentsLayout = new QHBoxLayout;
      auto llayout = new QVBoxLayout;
        llayout->addWidget(_viewer);
        llayout->addLayout(_statsLayout);
      _contentsLayout->addLayout(llayout);
      _contentsLayout->addLayout(_genesLayout);
      _contentsLayout->addLayout(_neuralLayout);

    QHBoxLayout *buttonsLayout = new QHBoxLayout;
    buttonsLayout->addWidget(filler(false));
    buttonsLayout->addWidget(_brainButton, 0, Qt::AlignCenter);
    buttonsLayout->addWidget(_saveButton, 0, Qt::AlignCenter);
    buttonsLayout->addWidget(_printButton, 0, Qt::AlignCenter);
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

QString defaultFilename (QLabel *firstname, QLabel *lastname) {
  return firstname->text().split(' ').back() + "_" + lastname->text();
}

void GeneticManipulator::saveSubjectBrain(void) {
  QString defaultFile = defaultFilename(_lFirstname, _lLastname);
  QString prefix = QFileDialog::getSaveFileName(
    this, "Base filename for " + defaultFile + " is...", defaultFile);

  saveSubjectBrain(prefix);
}

void GeneticManipulator::saveSubjectBrain(const QString &prefix) const {
  _subject->object().saveBrain(prefix.toStdString());
}

void GeneticManipulator::saveSubjectGenotype(void) {
  static const QString defaultExt =
    QString::fromStdString( _subject->object().genotype().extension());

  QString defaultFile = defaultFilename(_lFirstname, _lLastname);
  QString filename = QFileDialog::getSaveFileName(
    this, "Save " + defaultFile + " to...",
    defaultFile + defaultExt,
    "Splinoids (*" + defaultExt + ")");

  saveSubjectGenotype(filename);
}

void GeneticManipulator::saveSubjectGenotype(const QString &filename) const {
  if (filename.isEmpty()) return;
  _subject->saveGenotype(filename);
}

void GeneticManipulator::printSubjectPhenotype(void) {
  static const QString defaultExt = ".png";

  QString defaultFile = defaultFilename(_lFirstname, _lLastname);
  QString filename = QFileDialog::getSaveFileName(
    this, "Print " + defaultFile + " to...",
    defaultFile + defaultExt,
    "Rasterized (*.png)");

  printSubjectPhenotype(filename);
}

void GeneticManipulator::printSubjectPhenotype(const QString &filename) const {
  if (filename.isEmpty()) return;
  _subject->printPhenotype(filename);
}


void GeneticManipulator::buildViewer(void) {
  _viewer = new MiniViewer (this, _proxy);
}

QLayout* GeneticManipulator::buildBarsLayout (void) {
  QVBoxLayout *layout = new QVBoxLayout;
  QHBoxLayout *mainBarsLayout = new QHBoxLayout;
  QHBoxLayout *subBarsLayout = new QHBoxLayout;

  _sBars[0] = new PrettyBar (PrettyBar::BODY_ENERGY);
  _sBars[1] = new PrettyBar (PrettyBar::BODY_HEALTH);
  for (uint i=2; i<_sBars.size(); i++)
    _sBars[i] = new PrettyBar (PrettyBar::SPLINE_HEALTH);

  mainBarsLayout->addWidget(_sBars[0]);
  mainBarsLayout->addWidget(_sBars[1]);

  for (uint i=0; i<2*S_v; i++)  subBarsLayout->addWidget(_sBars[i+2]);

  layout->addLayout(mainBarsLayout);
  layout->addLayout(subBarsLayout);
  return layout;
}

void GeneticManipulator::buildStatsLayout(void) {
  QGridLayout *sl;
  _statsLayout = sl = new QGridLayout;

  sl->addWidget(_lFirstname = new QLabel, 0, 0, 1, 2);
  sl->addWidget(_lLastname = new QLabel, 0, 2, 1, 2);
  sl->addWidget(line(), 1, 0, 1, 4);

  QString headerStyleSheet = " font-size: 18px; font-weight: bold ";
  _lFirstname->setStyleSheet(headerStyleSheet);
  _lFirstname->setAlignment(Qt::AlignRight);
  _lLastname->setStyleSheet(headerStyleSheet);

  QGridLayout *othersLayout = new QGridLayout;
  sl->addLayout(othersLayout, 2, 0, 1, 4);

  int widths [] { 75, 40 };
  for (uint i=0; i<4; i++)
    othersLayout->setColumnMinimumWidth(i, widths[i%2]);

  _bSex = new QComboBox;
  _bSex->insertItem(simu::Critter::Sex::FEMALE, "female");
  _bSex->insertItem(  simu::Critter::Sex::MALE, "male");
  _bSex->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  connect(_bSex, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &GeneticManipulator::sexChanged);
  othersLayout->addWidget(_bSex, 0, 0, 1, 2);

  uint r = 0, c = 2;
  const auto addLRow =
    [this, othersLayout, &r, &c] (bool gene, QString l) {
    QLabel *label = new QLabel(l);
    label->setAlignment(Qt::AlignRight);
    if (!gene) {
      label->setStyleSheet("font-style: italic");
      label->setText(label->text() + " ");
    }
    othersLayout->addWidget(label, r, c);
    othersLayout->addWidget(_dataWidgets[l] = new QLabel, r, c+1);
    c+=2;
    if (c >= 4) c = 0, r++;
  };

                            addLRow(false, "Mass");
  addLRow(false, "Age");    addLRow(false, "Clock");
  addLRow( true, "Adult");  addLRow( true, "Old");
  addLRow(false, "Effcy");  addLRow(false, "Repro");
  addLRow(false, "LSpeed"); addLRow(false, "RSpeed");

  othersLayout->addWidget(line("Vision"), r++, 0, 1, 4);
  addLRow( true, "Angle");  addLRow(true, "Rotation");
  addLRow( true, "Width");  addLRow(true, "Precision");

  othersLayout->addWidget(line("Wellfare"), r++, 0, 1, 4);
  othersLayout->addLayout(buildBarsLayout(), r++, 0, 1, 4);

  sl->addWidget(filler(true), 3, 0, 1, 4);
}

void GeneticManipulator::buildGenesLayout(void) {
  const auto &cBounds = genotype::Critter::config_t::color_bounds();

  QVBoxLayout *gl;
  _genesLayout = gl = new QVBoxLayout;

#if CRITTER_SPLINES_COUNT > 0
  const auto &sBounds = genotype::Spline::config_t::dataBounds();
  gl->addWidget(line("Splines"));

  QGridLayout *splinesLayout = new QGridLayout;
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
    splinesLayout->addWidget(new QLabel(snames[j]), j, S_v);

  splinesLayout->addWidget(line("Dimorphism"), SD_v, 0, 1, -1);
#endif

  QGridLayout *colorsLayout = new QGridLayout;
  gl->addWidget(line("Colors"));
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
  uint lx = std::max(1u, S_v);
  for (uint j=0; j<2; j++) {
    GeneColorPicker *p;
    colorsLayout->addWidget(p = _bPickers[j]
                              = new GeneColorPicker (cBounds, 0, j+S_v),
                            2*j+1, 0, 1, lx);
  }

  for (uint j=0; j<4; j++) {
    if (j%2 == 0 && S_v == 0) continue;
    QLabel *n = new QLabel(cnames[j]);
    n->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    colorsLayout->addWidget(n, j, lx);
  }

  gl->addWidget(filler(true));
}

void GeneticManipulator::buildNeuralPanel(void) {
  _brainIO = new QWidget;
  QFormLayout *iol = new QFormLayout;
  iol->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

  iol->addRow(line("Neural I/O"));
  int rcells = 2 * (1+2*genotype::Vision::config_t::precisionBounds().max);
  iol->addRow("Vision", _rLabels = new ColorLabels(rcells, true));
  iol->addRow("Audition", _eLabels
             = new ColorLabels(2*(simu::Critter::VOCAL_CHANNELS+1), true));

  iol->addRow(line(""));

  QGridLayout *g = new QGridLayout;
  g->addWidget(new QLabel("Motors"), 0, 0, 1, 2, Qt::AlignCenter);
  g->addWidget(_mSliders[0] = new LabeledSlider(-1, 1), 1, 0);
  g->addWidget(_mSliders[1] = new LabeledSlider(-1, 1), 1, 1);

  g->addWidget(new QLabel("Clock"), 0, 2, 1, 1, Qt::AlignCenter);
  auto ml = new QHBoxLayout;
  ml->addWidget(_cLabels[0] =  new QLabel);
  ml->addWidget(_mSliders[2] = new LabeledSlider(-1, 1));
  ml->addWidget(_cLabels[1] = new QLabel);
  g->addLayout(ml, 1, 2);

  iol->addRow(g);

  iol->addRow("Sound", _sLabels
             = new ColorLabels(simu::Critter::VOCAL_CHANNELS+1, false));

  _brainIO->setLayout(iol);

  QVBoxLayout *nl;
  _neuralLayout = nl = new QVBoxLayout;
  nl->addWidget(_brainIO);
  nl->addWidget(_brainPanel = new kgd::es_hyperneat::gui::ES_HyperNEATPanel);
}

void GeneticManipulator::updateWindowName (void) {
  QString name = _readonly ? "Splinoid Sheet" : "Genetic Manipulator";
  if (_subject) name += ": " + _subject->firstname();
  setWindowTitle(name);
}

void GeneticManipulator::setReadOnly(bool r) {
  for (auto &sd: _sSliders) for (auto *s: sd) s->setReadOnly(r);
  for (auto &da: _dSliders) for (auto *s: da) s->setReadOnly(r);
  for (auto &sc: _sPickers) for (auto *p: sc) p->setReadOnly(r);
  for (auto *p: _bPickers)  p->setReadOnly(r);
  _bSex->setEnabled(!r);

  _editButton->setText(r ? "Edit" : "Ok");

  updateWindowName();
}

void GeneticManipulator::setSubject(visu::Critter *s) {
  using SCritter = simu::Critter;
//  using VCritter = visu::Critter;

  _updating = true;

  _subject = s;

  _proxy->setObject(s);

  _saveButton->setEnabled(_subject);
  _printButton->setEnabled(_subject);

  if (_subject) {
    const simu::Critter &c = _subject->object();
    const genotype::Critter &g = c.genotype();
    updateShapeData();

    _lLastname->setText(_subject->lastname());

    _bSex->setEnabled(true && !_readonly);
    _bSex->setCurrentIndex(int(c.genotype().cdata.sex));

    const auto degrees = [] (float v) {
      return QString::number(180. * v / M_PI, 'f', 1);
    };

    set("Adult", &SCritter::matureAt);
    set("Old", &SCritter::oldAt);
    set("Angle", &SCritter::visionBodyAngle, degrees);
    set("Rotation", &SCritter::visionRelativeRotation, degrees);
    set("Width", &SCritter::visionWidth, degrees);
    set("Precision", &SCritter::visionPrecision,
        [] (float v) { return QString::number(v, 'f', 0); });

    auto rhs = c.retina().size()/2;
    auto lhs = _rLabels->size()/2;
    _rLabels->setEnabled(true);
    for (uint i=rhs; i<lhs; i++)
      for (uint j=0; j<2; j++)
        _rLabels->label(i + j * lhs)->setVisible(false);

    _eLabels->setEnabled(true);
    _sLabels->setEnabled(true);

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

    _cLabels[0]->setText(QString::number(100*g.minClockSpeed, 'f', 1));
    _mSliders.back()->setRange(100*g.minClockSpeed, 100*g.maxClockSpeed);
    _cLabels[1]->setText(QString::number(100*g.maxClockSpeed, 'f', 1));

    _cppn = std::make_unique<phenotype::CPPN>(
              phenotype::CPPN::fromGenotype(g.brain));
    _brainPanel->setData(g.brain, *_cppn, c.brain());

    updateShapeData();
    readCurrentStatus();

  } else {
    _lFirstname->setText(visu::Critter::NO_FIRSTNAME);
    _lLastname->setText(visu::Critter::NO_LASTNAME);

    _bSex->setEnabled(false);
    for (QLabel *l: _dataWidgets) l->setText("");

    _rLabels->setEnabled(false);
    _eLabels->setEnabled(false);
    _sLabels->setEnabled(false);

    for (uint i=0; i<S_v; i++) {
      for (uint j=0; j<SD_v; j++) _sSliders[i][j]->noValue();
      for (uint j=0; j<2; j++)    _dSliders[i][j]->noValue();

      for (uint j=0; j<2; j++)    _sPickers[i][j]->noValue();
    }

    for (uint j=0; j<2; j++)  _bPickers[j]->noValue();

    for (PrettyBar *b: _sBars)  b->noValue();

    for (auto s: _mSliders) s->noValue();
    for (auto l: _cLabels)  l->setText("");

    _cppn.release();
    _brainPanel->noData();
  }

  qDebug() << "selection name: " << _lFirstname->text() << _lLastname->text();

  updateWindowName();

  _updating = false;
}

void GeneticManipulator::updateShapeData(void) {
  using SCritter = simu::Critter;
  const SCritter &c = _subject->object();

  set("Mass", &SCritter::mass);

  _sBars[0]->setMaximum(float(c.bodyMaxHealth()), false);
  _sBars[1]->setMaximum(float(c.maxUsableEnergy()), false);

  for (uint i=0; i<S_v; i++)
    for (SCritter::Side s: {SCritter::Side::LEFT, SCritter::Side::RIGHT})
      _sBars[2+uint(s)*S_v+i]->setMaximum(float(c.splineMaxHealth(i, s)),
                                          c.destroyedSpline(i, s));

  _proxy->objectUpdated();
}

void GeneticManipulator::readCurrentStatus(void) {
  using SCritter = simu::Critter;

  const simu::Critter &c = _subject->object();

  const auto percent = [] (float v) {
    return QString::number(100 * v, 'f', 1) + "%";
  };

  QString firstname;
  if (c.isYouth())      firstname += "Young ";
  else if (c.isElder()) firstname += "Old ";
  firstname += _subject->firstname();
  _lFirstname->setText(firstname);

  set("Age", &SCritter::age, percent);
  set("Clock", &SCritter::clockSpeed, percent);
  set("Effcy", &SCritter::efficiency, percent);

  set("Repro", &SCritter::reproductionReadinessBrittle, percent);

  set("LSpeed", &SCritter::linearSpeed,
      [] (float v) { return QString::number(v, 'f', 2) + " m/s"; });
  set("RSpeed", &SCritter::angularSpeed,
      [] (float v) {
    return (v >= 0 ? "+" : "") + QString::number(v / (2*M_PI), 'f', 2) + " t/s";
  });

  const auto &r = c.retina();
//  for (uint i=0; i<r.size(); i++) _rLabels[i+1]->setValue(r[i]);
  auto rhs = r.size()/2;
  auto lhs = _rLabels->size()/2;
  for (uint i=0; i<rhs; i++)
    for (uint j=0; j<2; j++)
      _rLabels->label(i + j * lhs)->setValue(r[i+j*rhs]);

  const auto &e = c.ears();
  const auto &s = c.producedSound();
  static constexpr auto es = simu::Critter::VOCAL_CHANNELS+1;
  for (uint j=0; j<es; j++) {
    for (uint i=0; i<2; i++)
      _eLabels->label(j + i * es)->setValue(std::max(0.f, e[i*es+j]));
    _sLabels->label(j)->setValue(s[j]);
  }

  _sBars[0]->setValue(float(c.usableEnergy()));
  _sBars[1]->setValue(float(c.bodyHealth()));

  for (uint i=0; i<S_v; i++)
    for (SCritter::Side s: {SCritter::Side::LEFT, SCritter::Side::RIGHT})
      _sBars[2+uint(s)*S_v+i]->setValue(float(c.splineHealth(i, s)));

  _mSliders[0]->setValue(c.motorOutput(Motor::LEFT));
  _mSliders[1]->setValue(c.motorOutput(Motor::RIGHT));
  _mSliders[2]->setValue(100*c.clockSpeed());

  if (config::Visualisation::animateANN())
    _brainPanel->annViewer->updateAnimation();
}

void GeneticManipulator::toggleBrainAnimation(void) {
  if (config::Visualisation::animateANN())
    _brainPanel->annViewer->startAnimation();
  else
    _brainPanel->annViewer->stopAnimation();
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
