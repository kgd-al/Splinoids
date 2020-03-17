#ifndef GENETICMANIPULATOR_H
#define GENETICMANIPULATOR_H

#include <QGraphicsView>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>

#include "../visu/critter.h"

namespace gui {

struct CritterProxy;
struct GeneSlider;
struct GeneColorPicker;

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

private:
  MiniViewer *_viewer;
  CritterProxy *_proxy;
  visu::Critter *_subject;

  SplineSliders _sSliders;
  DimorphismSliders _dSliders;

  SplineColorPickers _sPickers;
  BodyColorPickers _bPickers;

  VisionSliders _vSliders;

  struct CritterData {
    QLabel *firstname, *lastname;
    QComboBox *sex;
    QLabel *mass;
  } _dataWidgets;

  QBoxLayout *_contentsLayout, *_innerLayout;
  QLayout *_genesLayout, *_statsLayout;
  QPushButton *_editButton, *_hideButton;

  bool _updating, _readonly;

public:
  GeneticManipulator(QWidget *parent = nullptr);

  void setSubject (visu::Critter *s);

  void keyReleaseEvent(QKeyEvent *e) override;

  void toggleReadOnly (void) {
    _readonly = !_readonly;
    setReadOnly(_readonly);
  }

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
};

} // end of namespace gui

#endif // GENETICMANIPULATOR_H
