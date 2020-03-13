#ifndef GENETICMANIPULATOR_H
#define GENETICMANIPULATOR_H

#include <QGraphicsView>
#include <QDialog>
#include <QRadioButton>

#include "critter.h"

namespace visu {

struct CritterProxy;
struct GeneSlider;

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

  static constexpr uint SD_v = std::tuple_size_v<SData>;
  static constexpr uint S_v = CGenome::SPLINES_COUNT;

  using SplineDataSliders = std::array<GeneSlider*, SD_v>;
  using SplineSliders = std::array<SplineDataSliders, S_v>;
  using DimorphismSliders = std::array<std::array<GeneSlider*, 2>, S_v>;

private:
  MiniViewer *_viewer;
  CritterProxy *_proxy;
  visu::Critter *_subject;

  SplineSliders _sSliders;
  DimorphismSliders _dSliders;
  QRadioButton *_sex [2];
  bool _updating;

public:
  GeneticManipulator(QWidget *parent = nullptr);

  void setSubject (visu::Critter *s);

  void keyReleaseEvent(QKeyEvent *e) override;

signals:
  void keyReleased (QKeyEvent *e);

private:
  void updateWindowName (void);

  CGenome& genome (void) {
    return _subject->object().genotype();
  }

  void updateSubject (void);
  void updateGeneValue (void);
  void sexChanged (void);
};

} // end of namespace visu

#endif // GENETICMANIPULATOR_H
