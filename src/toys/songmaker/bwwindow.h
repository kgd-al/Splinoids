#ifndef KGD_BWWINDOW_H
#define KGD_BWWINDOW_H

#include <QMap>
#include <QMainWindow>
#include <QCheckBox>
#include <QSplitter>

#include "../gui/es_hyperneatpanel.h"
#include "sound/visualizer.h"

namespace kgd::watchmaker {

namespace simu {

struct Individual {
  using Genome = genotype::ES_HyperNEAT;
  Genome genome;

  using CPPN = phenotype::CPPN;
  CPPN cppn;

  using ANN = phenotype::ANN;
  ANN ann;

  using Phenotype = sound::StaticData::NoteSheet;
  Phenotype phenotype;

  using ptr = std::unique_ptr<Individual>;
  static ptr random (rng::AbstractDice &d);
  static ptr fromFile (const stdfs::path &path, rng::AbstractDice &dice);
  static ptr mutated (const Individual &i, rng::AbstractDice &d);

private:
  Individual (const Genome &g, const CPPN &c, const ANN &a);

  static ptr fromGenome (const Genome &g);
};

} // end of namespace simu

namespace gui {

class BWWindow : public QMainWindow {
  Q_OBJECT
public:
  BWWindow(const stdfs::path &baseSavePath, uint seed,
           QWidget *parent = nullptr);

  static stdfs::path generateOutputFolder (const stdfs::path &base);

  void firstGeneration (const stdfs::path &baseGenome);
  void show (void);

  void depthSummary (void);

  enum Setting {
    AUTOPLAY, MANUAL_PLAY, STEP_PLAY,
    LOCK_SELECTION, PLAY, SELECT_NEXT,
    ANIMATE, FAST_CLOSE
  };
  Q_ENUM(Setting)

private:
  QSplitter *_splitter;
  kgd::es_hyperneat::gui::ES_HyperNEATPanel *_details;

  static constexpr uint N = 3;
  static_assert(N%2 == 1, "Grid size must be odd");

  using IPtr = simu::Individual::ptr;
  std::array<IPtr, N*N> _individuals;
  uint _generation;

  std::array<sound::Visualizer*, N*N> _visualizers;
  sound::Visualizer *_shown, *_selection;

  QMap<Setting, QCheckBox*> _settings;

  rng::FastDice _dice;

  stdfs::path _baseSavePath, _currentSavePath;

  struct Animation {
    int index, step;
  } _animation;

  void nextGeneration (uint index);
  void updateSavePath (void);

  void setIndividual(IPtr &&in, uint j, uint k);
  static void evaluateIndividual (IPtr &i, uint step, bool setPhenotype);
  void logIndividual(uint index, const stdfs::path &folder,
                     int level) const;

  int indexOf (const sound::Visualizer *v) const;

  bool eventFilter(QObject *watched, QEvent *event) override;
  void individualHoverEnter (uint index);
  void individualHoverLeave (uint index);
  void individualMouseClick (uint index);

  void showIndividualDetails (int index);
  void setSelectedIndividual (int index);

  void startVocalisation (uint index);
  void stopVocalisation (uint index);

  void startAnimateShownANN (void);
  void animateShownANN (void);
  void stopAnimateShownANN (void);

  void keyPressEvent(QKeyEvent *e) override;

  using QMainWindow::show;
  bool setting (Setting s) const;
  void saveSettings (void) const;
  void restoreSettings (void);

  void closeEvent(QCloseEvent *e);
};

} // end of namespace gui

} // end of namespace kgd::watchmaker

#endif // KGD_BWWINDOW_H
