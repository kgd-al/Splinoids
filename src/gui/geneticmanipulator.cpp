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

static constexpr const char *snames [] {
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

static constexpr const char *cnames [] {
  "SXX", "BXX", "SXY", "BXY"
};


struct ColorLabel : public QLabel {
  using Color = genotype::Color;

  ColorLabel (void) {
    setAutoFillBackground(true);
  }

  void noValue (void) {
    setColorValue ({0,0,0});
  }

  void setValue (const Color &c) {
    setColorValue(c);
  }

  void mouseReleaseEvent(QMouseEvent*) {
    qDebug() << "Want to change color...";
  }

protected:
  QColor color;

  void setColorValue (const Color &c) {
    color = toQt(c);
    QPalette p = palette();
    p.setColor(QPalette::Window, color);
    setPalette(p);
    update();
  }
};

struct GeneColorPicker : public ColorLabel {
  template <typename B>
  GeneColorPicker (const B &b, uint i, uint j)
    : i(i), j(j), min(b.min), max(b.max), readonly(false) {}

  void setReadOnly (bool r) {
    readonly = r;
    if (r)  setEnabled(false);
  }

  void noValue (void) {
    setColorValue ({.5,.5,.5});
    setEnabled(false);
  }

  void setGeneValue (const Color &c) {
    setColorValue(c);
    setEnabled(true && !readonly);
  }

  void mouseReleaseEvent(QMouseEvent*) {
    qDebug() << "Want to change color...";
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
      prettyFormat();
  }

  void setValue (float v) {
    QProgressBar::setValue(MAX * v / max);
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
  _proxy = new CritterProxy;

  buildViewer();
  buildStatsLayout();
  buildGenesLayout();

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

  _hideButton = new QPushButton ("Close");
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
  std::string cppn_f = (prefix + "_cppn.dot").toStdString();
  std::ofstream cppn_ofs (cppn_f);
  _subject->object().genotype().connectivity.toDot(cppn_ofs);

  std::string ann_f = (prefix + "_ann.dat").toStdString();
  std::ofstream ann_ofs (ann_f);
  genotype::HyperNEAT::phenotypeToDat(ann_ofs, _subject->object().brain());

//#if defined(unix) || defined(__unix__) || defined(__unix)
//  std::ostringstream cmd;
////  std::string cppn_pdf = (prefix + "_cppn.pdf").toStdString();
////  cmd << "dot " << cppn_f << " -Tpdf -o " << cppn_pdf << " >log 2>&1";
////  std::cerr << "Executing \"" << cmd.str() << "\"\n";
////  int res = system(cmd.str().c_str());
////  if (res == 0) std::cerr << "\tSuccess\n";
////  else          std::cerr << "\tFailed\n";

//  cmd.str("");
//  cmd << "./scripts/plot_brain.sh " << prefix.toStdString();
//  std::cerr << "Executing \"" << cmd.str() << "\"\n";
//  int res = system(cmd.str().c_str());
//  if (res == 0) std::cerr << "\tSuccess\n";
//  else          std::cerr << "\tFailed\n";
//#endif
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

QLayout* GeneticManipulator::buildRetinaLayout(void) {
  QHBoxLayout *l = new QHBoxLayout;
  l->setSpacing(0);

  uint max = 2 * (1+2*genotype::Vision::config_t::precisionBounds().max) + 1;
  _rLabels.resize(max);

  for (uint i=0; i<max; i++) {
    ColorLabel *cl = _rLabels[i] = new ColorLabel;
    if (i == 0) {
      cl->setText("Retina");
      cl->setAlignment(Qt::AlignCenter);
      cl->setStyleSheet("border: 1px solid gray");

    } else {
      cl->noValue();
      cl->hide();
    }
    l->addWidget(cl);
  }

  return _retinaLayout = l;
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
  othersLayout->addLayout(buildRetinaLayout(), r++, 0, 1, 4);

  othersLayout->addWidget(line("Wellfare"), r++, 0, 1, 4);
  othersLayout->addLayout(buildBarsLayout(), r++, 0, 1, 4);

  sl->addWidget(filler(true), 3, 0, 1, 4);
}

void GeneticManipulator::buildGenesLayout(void) {
  const auto &sBounds = genotype::Spline::config_t::dataBounds();
  const auto &cBounds = genotype::Critter::config_t::color_bounds();

  QVBoxLayout *gl;
  _genesLayout = gl = new QVBoxLayout;

  QGridLayout *splinesLayout = new QGridLayout;
  gl->addWidget(line("Splines"));
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
  for (uint j=0; j<2; j++) {
    GeneColorPicker *p;
    colorsLayout->addWidget(p = _bPickers[j]
                              = new GeneColorPicker (cBounds, 0, j+S_v),
                            2*j+1, 0, 1, S_v);
  }

  for (uint j=0; j<4; j++) {
    QLabel *n = new QLabel(cnames[j]);
    n->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    colorsLayout->addWidget(n, j, S_v);
  }

  gl->addWidget(filler(true));
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

  // Set appropriate layout directions
  _contentsLayout->setDirection(
    _readonly ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
  _innerLayout->setDirection(
    _readonly ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);

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

    _rLabels[0]->hide();
    for (uint i=1; i<=c.retina().size(); i++)  _rLabels[i]->show();
    for (int i=c.retina().size()+1; i<_rLabels.size(); i++) _rLabels[i]->hide();

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

    updateShapeData();
    readCurrentStatus();

  } else {
    _lFirstname->setText(visu::Critter::NO_FIRSTNAME);
    _lLastname->setText(visu::Critter::NO_LASTNAME);

    _bSex->setEnabled(false);
    for (QLabel *l: _dataWidgets) l->setText("");

    _rLabels[0]->show();
    for (int i=1; i<_rLabels.size(); i++)  _rLabels[i]->hide();

    for (uint i=0; i<S_v; i++) {
      for (uint j=0; j<SD_v; j++) _sSliders[i][j]->noValue();
      for (uint j=0; j<2; j++)    _dSliders[i][j]->noValue();

      for (uint j=0; j<2; j++)    _sPickers[i][j]->noValue();
    }

    for (uint j=0; j<2; j++)  _bPickers[j]->noValue();

    for (PrettyBar *b: _sBars)  b->noValue();
  }

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
  set("Repro", &SCritter::reproductionReadiness, percent);

  set("LSpeed", &SCritter::linearSpeed,
      [] (float v) { return QString::number(v, 'f', 2) + " m/s"; });
  set("RSpeed", &SCritter::angularSpeed,
      [] (float v) {
    return (v >= 0 ? "+" : "") + QString::number(v / (2*M_PI), 'f', 2) + " t/s";
  });

  const auto &r = c.retina();
  for (uint i=0; i<r.size(); i++) _rLabels[i+1]->setValue(r[i]);

  _sBars[0]->setValue(float(c.usableEnergy()));
  _sBars[1]->setValue(float(c.bodyHealth()));

  for (uint i=0; i<S_v; i++)
    for (SCritter::Side s: {SCritter::Side::LEFT, SCritter::Side::RIGHT})
      _sBars[2+uint(s)*S_v+i]->setValue(float(c.splineHealth(i, s)));
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
