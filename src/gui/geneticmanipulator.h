#ifndef GENETICMANIPULATOR_H
#define GENETICMANIPULATOR_H

#include <QGraphicsView>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QProgressBar>

#include "kgd/eshn/gui/es_hyperneatpanel.h"
#include "../visu/critter.h"

namespace gui {

struct CritterProxy;
struct LabeledSlider;
struct GeneSlider;
struct ColorLabel;
struct GeneColorPicker;
struct ColorLabels;
struct PrettyBar;

class MiniViewer : public QGraphicsView {
public:
  MiniViewer (QWidget *parent, CritterProxy *proxy);

  virtual ~MiniViewer (void);

  void resizeEvent(QResizeEvent *e) override;
};

class GeneticManipulator : public QDialog {
  Q_OBJECT
public:
  using SData = genotype::Spline::Data;
  using CGenome = genotype::Critter;
  using Sex = CGenome::Sex;

  static constexpr uint SD_v = std::tuple_size_v<SData>;
  static constexpr uint S_v = CGenome::SPLINES_COUNT;

  using SplineDataSliders = std::array<GeneSlider*, SD_v>;
  using SplineSliders = std::array<SplineDataSliders, S_v>;

#ifdef USE_DIMORPHISM
  using DimorphismSliders = std::array<std::array<GeneSlider*, 2>, S_v>;

  using SplineColorPickers = std::array<std::array<GeneColorPicker*, 2>, S_v>;
  using BodyColorPickers = std::array<GeneColorPicker*, 2>;
#else
  using SplineColorPickers = std::array<GeneColorPicker*, S_v>;
  using BodyColorPicker = GeneColorPicker*;
#endif

  using VisionSliders = std::array<GeneSlider*, 4>;

  using StatusBars = std::array<PrettyBar*, 1+1+2*S_v>;

private:
  MiniViewer *_viewer;
  CritterProxy *_proxy;
  visu::Critter *_subject;

  SplineSliders _sSliders;

#ifdef USE_DIMORPHISM
  DimorphismSliders _dSliders;
#endif

  SplineColorPickers _sPickers;

#ifdef USE_DIMORPHISM
  BodyColorPickers _bPickers;
#else
  BodyColorPicker _bPicker;
#endif

  VisionSliders _vSliders;

  StatusBars _sBars;

  QLabel *_lFirstname, *_lLastname;
  QComboBox *_bSex;
  QMap<QString, QLabel*> _dataWidgets;
  ColorLabels *_rLabels, *_eLabels, *_sLabels;
  std::array<LabeledSlider*, 3> _mSliders;
  std::array<QLabel*, 2> _cLabels;

  QHBoxLayout *_contentsLayout;
  QBoxLayout *_retinaLayout, *_earsLayout;
  QLayout *_genesLayout, *_statsLayout, *_neuralLayout;

  QWidget *_brainIO;
  kgd::es_hyperneat::gui::ES_HyperNEATPanel *_brainPanel;
  std::unique_ptr<phenotype::CPPN> _cppn;

  QPushButton *_brainButton, *_saveButton, *_printButton,
              *_editButton, *_hideButton;

  bool _updating, _readonly;

public:
  GeneticManipulator(QWidget *parent = nullptr);

  void setSubject (visu::Critter *s);
  void updateShapeData (void);

  void keyReleaseEvent(QKeyEvent *e) override;

  void toggleShow (void) {
    setVisible(!isVisible());
  }

  void toggleReadOnly (void) {
    _readonly = !_readonly;
    setReadOnly(_readonly);
  }

  auto* brainPanel (void) {
    return _brainPanel;
  }

  auto brainIO (void) {
    return _brainIO;
  }

  void readCurrentStatus (void);
  void toggleBrainAnimation (void);

  void saveSubjectBrain (void);
  void saveSubjectBrain (const QString &prefix) const;

  void saveSubjectGenotype (void);
  void saveSubjectGenotype (const QString &filename) const;

  void printSubjectPhenotype (void);
  void printSubjectPhenotype (const QString &filename) const;

  void updateVisu (void);

signals:
  void keyReleased (QKeyEvent *e);

private:
  void updateWindowName (void);

  CGenome& genome (void) {
    return _subject->object().genotype();
  }

  void setReadOnly (bool r);

  void updateSubject (void);
  void updateGeneValue (void);
  void sexChanged (int i);

  void buildViewer (void);
  void buildGenesLayout (void);
  void buildStatsLayout (void);
  void buildNeuralPanel (void);

  QLayout* buildBarsLayout (void);

  template <typename T>
  void set (const QString &l, T (simu::Critter::*f) (void) const,
            const std::function<QString(float)> &formatter = [] (float v) {
              return QString::number(v, 'f', 2); }) {

      _dataWidgets[l]->setText(formatter((_subject->object().*f)()));
  }
};

} // end of namespace gui

#endif // GENETICMANIPULATOR_H
