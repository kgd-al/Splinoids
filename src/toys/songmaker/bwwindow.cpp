#include <QGridLayout>
#include <QHoverEvent>
#include <QMessageBox>
#include <QSettings>
#include <QButtonGroup>
#include <QApplication>
#include <QGroupBox>

#include "bwwindow.h"
#include "config.h"
#include "qflowlayout.h"

#include <QDebug>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QToolButton>
#include <QMetaEnum>

namespace kgd::watchmaker {

namespace simu {

Individual::Individual(const Genome &g, const CPPN &c, const ANN &a)
  : genome(g), cppn(c), ann(a) {}

Individual::ptr Individual::fromGenome (const genotype::ES_HyperNEAT &genome) {
  using Config = config::WatchMaker;

  auto cppn = phenotype::CPPN::fromGenotype(genome);

  phenotype::ANN::Coordinates inputs, outputs;
  static constexpr auto C = sound::StaticData::CHANNELS;
  for (uint i=0; i<C; i++) {
    if (Config::audition() != Audition::NONE)
      inputs.push_back( { 2*float(i) / (C-1) - 1.f, -1 });
    outputs.push_back({ 2*float(i) / (C-1) - 1.f, +1 });
  }

  if (Config::tinput() != TemporalInput::NONE)  inputs.push_back({0, -.5});

  auto ann = phenotype::ANN::build(inputs, outputs, cppn);

  return ptr(new Individual(genome, cppn, ann));
}

Individual::ptr Individual::fromFile(const stdfs::path &path,
                                     rng::AbstractDice &dice) {
  auto ext = path.extension();
  return fromGenome(
    (ext == ".dot") ? genotype::ES_HyperNEAT::fromDotFile(path, dice)
                    : genotype::ES_HyperNEAT::fromGenomeFile(path));
}

Individual::ptr Individual::random(rng::AbstractDice &d) {
  return fromGenome(genotype::ES_HyperNEAT::random(d));
}

Individual::ptr Individual::mutated (const Individual &i, rng::AbstractDice &d){
  auto genome = i.genome;
#ifndef NDEBUG
  assertEqual(i.genome, genome, true);
#endif
  genome.mutate(d);
  return fromGenome(genome);
}

} // end of namespace simu

namespace gui {

BWWindow::BWWindow(const stdfs::path &baseSavePath, uint seed, QWidget *parent)
  : QMainWindow(parent), _dice(seed), _baseSavePath(baseSavePath) {
//  _sounds = new SoundGenerator(this);

  std::cerr << "Using seed: " << seed << " -> " << _dice.getSeed() << "\n";

  _splitter = new QSplitter;
  _splitter->setObjectName("bw::splitter");

  auto holder = new QWidget;
  auto layout = new QGridLayout;

  for (uint i=0; i<N; i++) {
    for (uint j=0; j<N; j++) {
      uint ix = i*N+j;
      auto v = new sound::Visualizer(i*N+j);
      layout->addWidget(_visualizers[ix] = v, i, j);
      sound::MidiWrapper::setInstrument(ix,
                                        config::WatchMaker::midiInstrument());
    }
  }
  for (auto v: _visualizers)
    v->QWidget::installEventFilter(this);
  _shown = nullptr;
  _selection = nullptr;

  auto *clayout = new FlowLayout;
  {
    QLayout *subLayout = nullptr;
    auto newFrame = [&subLayout, clayout] (const QString &title) {
      auto w = new QGroupBox(title);
      w->setFlat(true);
      w->setAlignment(Qt::AlignLeft);
      subLayout = new FlowLayout;
      w->setLayout(subLayout);
      clayout->addWidget(w);
    };
    auto add = [&subLayout] (QWidget *w) {
      subLayout->addWidget(w);
    };
    QMetaEnum e = QMetaEnum::fromType<Setting>();
    for (int i=0; i<e.keyCount(); i++) {
      Setting s = Setting(e.value(i));
      if (s == AUTOPLAY)  newFrame("Sound");
      else if (s == LOCK_SELECTION) newFrame("LMouse");
      else if (s == ANIMATE) newFrame("Other");

      QString name = QString(e.key(i)).replace("_", " ").toLower();
      name[0] = name[0].toUpper();

      QCheckBox *c;
      add(_settings[s] = c = new QCheckBox (name));
      c->setFocusPolicy(Qt::NoFocus);
    }

    QButtonGroup *gp = new QButtonGroup(this);
    for (Setting s: {AUTOPLAY,MANUAL_PLAY,STEP_PLAY})
      gp->addButton(_settings.value(s));

    QButtonGroup *gm = new QButtonGroup(this);
    for (Setting s: {LOCK_SELECTION,SELECT_NEXT,PLAY})
      gm->addButton(_settings.value(s));
  }
  layout->addLayout(clayout, N, 0, 1, N, Qt::AlignCenter);

  holder->setLayout(layout);
  _splitter->addWidget(holder);
  _splitter->addWidget(_details =
    new kgd::es_hyperneat::gui::ES_HyperNEATPanel);
  setCentralWidget(_splitter);

  _animation.index = -1;
  _animation.step = -1;
}

stdfs::path BWWindow::generateOutputFolder (const stdfs::path &base) {
  stdfs::path path = base;
  if (path.empty())  path = "./tmp/";

  std::ostringstream oss;
  oss << utils::CurrentTime("%Y-%m-%d_%H-%M-%S");
  auto link = path / "last";
  path /= oss.str();
  stdfs::create_directories(path);
  path = stdfs::canonical(path);

  if (stdfs::exists(link))  stdfs::remove(link);
  stdfs::create_directory_symlink(path, link);

  return path;
}

struct GenerationDialog : public QDialog {
  GenerationDialog (uint generation, bool newGeneration,
                    QWidget *parent) : QDialog(parent) {
    setWindowTitle("Please wait");
    setModal(true);
    auto *layout = new QVBoxLayout;
    QString msg;
    if (newGeneration)
      msg = QString("Spawning generation %1").arg(generation);
    else
      msg = QString("Spawning alternatives for generation %1").arg(generation);
    layout->addWidget(new QLabel(windowTitle()));
    layout->addWidget(new QLabel(msg));
    setLayout(layout);
  }
};


#ifdef DEBUG_QUADTREE
static void quadtreeDebugPrefix (const stdfs::path &base, uint i, uint j) {
  std::ostringstream oss;
  oss << "i" << i << "_j" << j << "/quadtrees";
  stdfs::path prefix = base / oss.str();
  stdfs::create_directories(prefix.parent_path());
  quadtree_debug::debugFilePrefix(base / oss.str());
  std::cerr << "quadtree_debug::debugFilePrefix(): "
            << quadtree_debug::debugFilePrefix() << std::endl;
}
#endif

void BWWindow::depthSummary(void) {
#if 1 && ESHN_SUBSTRATE_DIMENSION == 3
  std::ofstream ofs ("tmp/depth_debug.log", std::ios_base::out
                                            | (_generation == 0 ?
                                                 std::ios_base::trunc
                                               : std::ios_base::app));

  if (_generation == 0)
    ofs << "G IX D N E\n";

  std::array<uint, N*N> depths;
  uint maxDepth = 0;
  for (uint i=0; i<N; i++) {
    for (uint j=0; j<N; j++) {
      uint d = 0;
      uint IX = i*N+j;
      uint edges = 0;
      const auto &ann = _individuals[IX]->ann;
      for (const auto &n: ann.neurons()) {
        d = std::max(d, n->depth);
        edges += n->links().size();
      }

      depths[IX] = d;
      if (maxDepth < d)
        maxDepth = d;

      ofs << _generation << " " << IX << " " << d
          << " " << ann.neurons().size() << " " << edges << "\n";
    }
  }

  ofs << "\n";

  uint w = std::ceil(std::log10(maxDepth));
  std::ostringstream oss;
  oss << std::setfill('#')
      << "\\  ";
  for (uint i=0; i<N; i++)
    oss << " " << std::setw(w) << i;
  oss << "\n\n";
  for (uint i=0; i<N; i++) {
    oss << std::setw(w) << i << "  ";
    for (uint j=0; j<N; j++)
      oss << " " << std::setw(w) << depths[i*N+j];
    oss << "\n";
  }
  std::cout << oss.str() << std::endl;
#endif
}

void BWWindow::firstGeneration(const stdfs::path &baseGenome) {
  _generation = 0;
  updateSavePath();

  if (baseGenome.empty()) {
    for (uint i=0; i<N; i++) {
      for (uint j=0; j<N; j++) {
#ifdef DEBUG_QUADTREE
        quadtreeDebugPrefix(_currentSavePath, i, j);
#endif
        setIndividual(simu::Individual::random(_dice), i, j);
      }
    }

  } else {
#ifdef DEBUG_QUADTREE
    quadtreeDebugPrefix(_currentSavePath, N/2, N/2);
#endif
    for (uint i=0; i<N; i++) {
      for (uint j=0; j<N; j++) {
#ifdef DEBUG_QUADTREE
        quadtreeDebugPrefix(_currentSavePath, i, j);
#endif
        setIndividual(simu::Individual::fromFile(baseGenome, _dice), i, j);
      }
    }
  }

  uint empty = 0;
  for (const auto &i: _individuals) if (i->ann.empty()) empty++;
  std::cout << "Empty phenotypes: " << empty << " / " << (N*N) << " ("
            << 100. * empty / (N*N) << "%)" << std::endl;

  if (setting(LOCK_SELECTION))
        setSelectedIndividual(N*N/2);
  else  showIndividualDetails(N*N/2);

  depthSummary();
}

void BWWindow::nextGeneration(uint index) {
  setSelectedIndividual(-1);
  showIndividualDetails(-1);

  if (index != N*N/2) _generation++;

  /// TODO Put a (non-freezing) blocking dialog on that

  std::cerr << "Processing generation " << _generation << "\n";
  updateSavePath();

  IPtr parent = std::move(_individuals[index]);
  uint mid = N/2;
  for (uint i=0; i<N; i++) {
    for (uint j=0; j<N; j++) {
      if (i == mid && j == mid)  continue;

#ifdef DEBUG_QUADTREE
      quadtreeDebugPrefix(_currentSavePath, i, j);
#endif

      setIndividual(simu::Individual::mutated(*parent, _dice), i, j);
      std::cerr << ".";
    }
  }
  std::cerr << "\n";

  setIndividual(std::move(parent), mid, mid);
  showIndividualDetails(N*N/2);
//  QCursor::setPos(mapToGlobal(_visualizers[N*N/2]->geometry().center()));

  depthSummary();
}

void BWWindow::updateSavePath(void) {
  std::ostringstream oss;
  oss << "gen" << _generation;
  _currentSavePath = _baseSavePath / oss.str();
  stdfs::create_directories(_currentSavePath);
}

void BWWindow::setIndividual(IPtr &&in, uint j, uint k) {
  const auto ix = j*N+k;
  IPtr &i = _individuals[ix] = std::move(in);

  for (uint n=0; n<sound::StaticData::NOTES; n++)
    evaluateIndividual(i, n, true);

  _visualizers[ix]->setNoteSheet(i->phenotype);

//  using utils::operator<<;
//  std::cerr << "Note sheet for [" << j << "," << k << "]: " << notes << "\n";

  if (ix != N*N/2 || _generation == 0) {
    // Don't save last gen champion again
    std::ostringstream oss;
    oss << "i" << j << "_j" << k;
    logIndividual(ix, _currentSavePath / oss.str(),
                  config::WatchMaker::dataLogLevel());
  }
}

void BWWindow::evaluateIndividual(IPtr &i, uint step, bool setPhenotype) {
  using Config = config::WatchMaker;
  static constexpr auto C = sound::StaticData::CHANNELS;

  auto &ann = i->ann;
  auto &notes = i->phenotype;
  auto inputs = ann.inputs(), outputs = ann.outputs();

  if (step > 0) {
    uint off = (step-1)*C;

    switch (Config::audition()) {
    case Audition::NONE:  break;
    case Audition::SELF:
      std::copy(notes.begin()+off, notes.begin()+off+C, inputs.begin());
      break;
    case Audition::COMMUNITY:
      assert(false);
      break;
    }

    switch (Config::tinput()) {
    case TemporalInput::NONE: break;
    case TemporalInput::LINEAR:
      inputs.back() = float(step) / (sound::StaticData::NOTES-1);
      break;
    case TemporalInput::SINUSOIDAL:
//      std::cerr << "isin(" << step << ") = "
//                << std::sin(2 * M_PI * step * sound::StaticData::NOTE_DURATION / 4)
//                << " = PI * " << step << " * "
//                << sound::StaticData::NOTE_DURATION << " / 4\n";
      inputs.back() =
          std::sin(2 * M_PI * step * sound::StaticData::NOTE_DURATION / 4.);
      break;
    }
  }

  ann(inputs, outputs, i->genome.substeps);

  if (setPhenotype) {
    if (!config::WatchMaker::polyphonic()) {
      // Only take maximally activated neuron in monophonic mode
      uint i=0;
      float max = 0;
      for (uint c = 0; c<C; c++) {
        if (max < outputs[c]) {
          i = c;
          max = outputs[c];
        }
      }
      for (uint c = 0; c<C; c++)  if (c != i) outputs[c] = 0;
    }
    std::copy(outputs.begin(), outputs.end(), notes.begin()+step*C);
  }
}

void BWWindow::logIndividual(uint index, const stdfs::path &f,
                             int level) const {
  if (level <= 0) return;
  stdfs::create_directories(f);

  /// TODO Also log plain note sheet

  using namespace kgd::es_hyperneat::gui;
  const auto &i = *_individuals[index];
  if (level >= 1) i.genome.toFile(f / "genome");
  if (level >= 3) i.genome.cppn.render_gvc_graph(f / "cppn_gvc.png");
  using GV = GraphViewer;
  if (level >= 2) GV::render<cppn::Viewer>(i.genome.cppn, f / "cppn_qt.pdf");
  if (level >= 3) i.ann.render_gvc_graph(f / "ann_gvc.png");
#if ESHN_SUBSTRATE_DIMENSION == 2
  if (level >= 2) GV::render<ann::Viewer>(i.ann, f / "ann_qt.pdf");
#endif

  const auto &v = *_visualizers[index];
  if (level >= 1) v.render(f / "song.png");
  if (level >= 1) sound::MidiWrapper::writeMidi(i.phenotype, f / "song.mid");
//  if (level >= 2) v.sound().generateWav(f / "song.wav");
}

int BWWindow::indexOf (const sound::Visualizer *v) const {
  int index = -1;
  for (uint i=0; i<_visualizers.size() && index < 0; i++)
    if (_visualizers[i] == v)
      index = i;
  return index;
}

bool BWWindow::eventFilter(QObject *watched, QEvent *event) {
  static const QList<QEvent::Type> types {
    QEvent::HoverEnter, QEvent::HoverLeave,
    QEvent::MouseButtonRelease, QEvent::MouseButtonDblClick
  };

  auto *v = dynamic_cast<sound::Visualizer*>(watched);
  if (!v) return false;

  if (!types.contains(event->type())) return false;

  int index = indexOf(v);
  if (index < 0)  return false;

  switch (event->type()) {
  case QEvent::HoverEnter:
    individualHoverEnter(index);
    break;
  case QEvent::HoverLeave:
    individualHoverLeave(index);
    break;
  case QEvent::MouseButtonRelease:
    individualMouseClick(index);
    break;
//  case QEvent::MouseButtonDblClick:
//    individualMouseDoubleClick(index);
//    break;
  default:  break;
  }

  return false;
}

void BWWindow::keyPressEvent(QKeyEvent *e) {
  auto s = -1;
  if (e->modifiers() & Qt::CTRL)        s = PLAY;
  else if (e->modifiers() & Qt::SHIFT)  s = LOCK_SELECTION;
  else if (e->modifiers() & Qt::ALT)    s = SELECT_NEXT;
  if (s >= 0) _settings.value(Setting(s))->setChecked(true);

  if (N == 3 && e->modifiers() == Qt::KeypadModifier) {
    int ix = e->key() - Qt::Key_1;
    if (0 > ix || ix > 8) return;

    ix = N * (N - ix/N - 1) + ix%N;

    showIndividualDetails(ix);
    individualMouseClick(ix);
    return;
  }

  auto v = _selection;
  if (!v) v = dynamic_cast<sound::Visualizer*>(childAt(QCursor::pos()));
  if (!v) return;

  int index = indexOf(v);
  if (index < 0)  return;

  switch (e->key()) {
  case Qt::Key_Space: startVocalisation(index); break;
  case Qt::Key_Escape: stopVocalisation(index); break;
  }
}

void BWWindow::individualHoverEnter(uint index) {
  showIndividualDetails(index);
  if (setting(AUTOPLAY))  startVocalisation(index);
}

void BWWindow::individualHoverLeave(uint index) {
  if (setting(AUTOPLAY) || _selection == nullptr)  stopVocalisation(index);
}

void BWWindow::individualMouseClick(uint index) {
  if (setting(LOCK_SELECTION))    setSelectedIndividual(index);
  else if (setting(SELECT_NEXT))  nextGeneration(index);
  else if (setting(PLAY) && !setting(AUTOPLAY))
    startVocalisation(index);
}

void BWWindow::showIndividualDetails(int index) {
  if (_selection)  return;

  if (_shown) _shown->setHighlighted(false);

  if (index < 0)
    _details->noData();

  else {
    auto &i = _individuals.at(index);
    _details->setData(i->genome, i->cppn, i->ann);

#if ESHN_SUBSTRATE_DIMENSION == 3
    _details->annViewer->depthDebugDraw(true);
#endif

    _shown = _visualizers[index];
    _shown->setHighlighted(true);

//    using utils::operator<<;
//    std::cerr << "Details of individual " << index << ":\n"
//              << " cppn: " << i->genome.cppn << "\n"
//              << " phenotype: " << i->phenotype << "\n";
  }
}

void BWWindow::setSelectedIndividual(int index) {
  sound::Visualizer *v = (index >= 0) ? _visualizers[index] : nullptr;
  bool deselect = (v == nullptr || _selection == v);

  if (_selection) _selection = nullptr;
  if (!deselect) {
    showIndividualDetails(index);
    _selection = v;
  }
}

void BWWindow::startVocalisation(uint index) {
  auto v = _visualizers[index];
  if (_selection != nullptr && _selection != v) return;

  if (_animation.index < 0) {
    _animation.index = index;
    _animation.step = 0;
    startAnimateShownANN();
  }

  if (setting(AUTOPLAY))
    v->vocaliseToAudioOut(sound::StaticData::LOOP);
  else if (setting(MANUAL_PLAY))
    v->vocaliseToAudioOut(sound::StaticData::ONE_PASS);
  else if (setting(STEP_PLAY))
    v->vocaliseToAudioOut(sound::StaticData::ONE_NOTE);
}

void BWWindow::stopVocalisation(uint index) {
  auto v = _visualizers[index];
  if ((setting(PLAY) || setting(AUTOPLAY))
       && ((_selection == nullptr) || (_selection == v))) {
    v->stopVocalisationToAudioOut();
    if (setting(ANIMATE)) stopAnimateShownANN();
  }
}

void BWWindow::startAnimateShownANN(void) {
  if (!setting(ANIMATE))  return;

  connect(_visualizers[_animation.index], &sound::Visualizer::notifyNote,
          this, &BWWindow::animateShownANN);
  _individuals[_animation.index]->ann.reset();

  _details->annViewer->startAnimation();

  std::cerr << "Started animation of individual " << _animation.index << "\n";
}

void BWWindow::animateShownANN(void) {
  if (!setting(ANIMATE))  return;

  assert(_animation.index >= 0);
  assert(_animation.step >= 0);

  std::cerr << "animation step " << _animation.step << " of individual "
            << _animation.index << "\n";
  evaluateIndividual(_individuals[_animation.index], _animation.step, false);

#if ESHN_SUBSTRATE_DIMENSION == 2
  _details->annViewer->updateAnimation();
#endif

  _details->neuronViewer->updateState();

  _animation.step = (_animation.step + 1) % sound::StaticData::NOTES;
}

void BWWindow::stopAnimateShownANN(void) {
  if (!setting(ANIMATE))  return;

  std::cerr << "Stopped animation of individual " << _animation.index << "\n";

#if ESHN_SUBSTRATE_DIMENSION == 2
  auto &av = *_details->annViewer;
  if (av.isAnimating()) {
    assert(_animation.index >= 0);
    disconnect(_visualizers[_animation.index], &sound::Visualizer::notifyNote,
               this, &BWWindow::animateShownANN);
    av.stopAnimation();
    _animation.index = -1;
    _animation.step = -1;
  }
#endif
}

void BWWindow::show(void) {
  restoreSettings();
  QMainWindow::show();
}

bool BWWindow::setting(Setting s) const {
  return _settings.value(s)->isChecked();
}

void BWWindow::saveSettings(void) const {
  QSettings settings;
  settings.setValue("geom", saveGeometry());
  kgd::gui::save(settings, _splitter);

  settings.beginGroup("settings");
  QMetaEnum e = QMetaEnum::fromType<Setting>();
  for (int i=0; i<e.keyCount(); i++)
    settings.setValue(e.key(i), setting(Setting(e.value(i))));
  settings.endGroup();

  settings.setValue("selection", indexOf(_selection));
}

void BWWindow::restoreSettings(void) {
  QSettings settings;
  kgd::gui::restore(settings, _splitter);
  restoreGeometry(settings.value("geom").toByteArray());

  settings.beginGroup("settings");
  QMetaEnum e = QMetaEnum::fromType<Setting>();
  for (int i=0; i<e.keyCount(); i++)
    _settings[Setting(e.value(i))]->setChecked(settings.value(e.key(i))
                                                       .toBool());
  settings.endGroup();

  int selection = settings.value("selection").toInt();
  if (selection >= 0 && (uint)selection < _individuals.size())
    setSelectedIndividual(selection);
}

void BWWindow::closeEvent(QCloseEvent *e) {
  if (setting(FAST_CLOSE)
      || QMessageBox::question(this, "Closing", "Confirm closing?",
                               QMessageBox::Yes, QMessageBox::No)
        == QMessageBox::Yes) {
    saveSettings();
    e->accept();

  } else
    e->ignore();
}

} // end of namespace gui

} // end of namespace kgd::watchmaker
