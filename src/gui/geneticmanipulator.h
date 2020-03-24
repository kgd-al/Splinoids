#ifndef GENETICMANIPULATOR_H
#define GENETICMANIPULATOR_H

#include <QGraphicsView>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QProgressBar>

#include "../visu/critter.h"

namespace gui {

struct CritterProxy;
struct GeneSlider;
struct GeneColorPicker;
struct ColorLabel;
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
  using DimorphismSliders = std::array<std::array<GeneSlider*, 2>, S_v>;

  using SplineColorPickers = std::array<std::array<GeneColorPicker*, 2>, S_v>;
  using BodyColorPickers = std::array<GeneColorPicker*, 2>;

  using VisionSliders = std::array<GeneSlider*, 4>;

  using StatusBars = std::array<PrettyBar*, 1+1+2*S_v>;

private:
  MiniViewer *_viewer;
  CritterProxy *_proxy;
  visu::Critter *_subject;

  SplineSliders _sSliders;
  DimorphismSliders _dSliders;

  SplineColorPickers _sPickers;
  BodyColorPickers _bPickers;

  VisionSliders _vSliders;

  StatusBars _sBars;

  QLabel *_lFirstname, *_lLastname;
  QComboBox *_bSex;
  QMap<QString, QLabel*> _dataWidgets;
  QVector<ColorLabel*> _rLabels;

  QBoxLayout *_contentsLayout, *_innerLayout, *_retinaLayout;
  QLayout *_genesLayout, *_statsLayout;
  QPushButton *_saveButton, *_printButton, *_editButton, *_hideButton;

  bool _updating, _readonly;

public:
  GeneticManipulator(QWidget *parent = nullptr);

  void setSubject (visu::Critter *s);

  void keyReleaseEvent(QKeyEvent *e) override;

  void toggleReadOnly (void) {
    _readonly = !_readonly;
    setReadOnly(_readonly);
  }

  void readCurrentStatus (void);

  void saveSubjectGenotype (void);
  void saveSubjectGenotype (const QString &filename) const;

  void printSubjectPhenotype (void);
  void printSubjectPhenotype (const QString &filename) const;

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

  QLayout* buildBarsLayout (void);
  QLayout* buildRetinaLayout (void);

  template <typename T>
  void set (const QString &l, T (simu::Critter::*f) (void) const,
            const std::function<QString(float)> &formatter = [] (float v) {
              return QString::number(v, 'f', 2); }) {

      _dataWidgets[l]->setText(formatter((_subject->object().*f)()));
  }
};

} // end of namespace gui

#endif // GENETICMANIPULATOR_H
