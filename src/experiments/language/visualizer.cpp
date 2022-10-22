#include <QApplication>
#include <QMainWindow>
#include <QSettings>
#include <QTimer>
#include <QSplitter>
#include <QDockWidget>

#include "kgd/external/cxxopts.hpp"

#include "indevaluator.h"
#include "../../gui/mainview.h"

#include <QDebug>
#include "../../visu/config.h"
#include <QPrinter>

#include "kgd/eshn/gui/ann/2d/viewer.h"

Q_DECLARE_METATYPE(BrainDead)

//#include "config/dependencies.h"

#ifdef WITH_MIDI
#include "MidiFile.h"
struct MidiFileWrapper {
  static constexpr auto C = simu::Critter::VOCAL_CHANNELS;
  using Notes = std::array<float, C>;

private:
  static constexpr auto TPQ = 96;

  static constexpr uint BASE_OCTAVE = 1;
  static constexpr uint BASE_A = 21 + 12 * BASE_OCTAVE;

  Notes previousNotes;

  smf::MidiFile midifile;

  static uchar key (int index) {   return BASE_A+12*index;  }
  static uchar velocity (float volume) {
    assert(-1 <= volume && volume <= 1);
    return std::round(127*std::max(0.f, volume));
  }

public:
  MidiFileWrapper (int instrument) {
    midifile.setTicksPerQuarterNote(TPQ);

    const auto TEMPO = 60 * config::Simulation::ticksPerSecond();
    midifile.addTempo(0, 0, TEMPO);
    midifile.addTimbre(0, 0, 0, instrument);

    previousNotes.fill(0);
  }

  void process (uint timestep, const Notes &notes) {
    int t = TPQ * timestep;  /// TODO Debug
//    std::cerr << "t(" << n << ") = " << t << "\n";
    for (uint c=0; c<C; c++) {
      float fn = notes[c];
      uchar cn = velocity(fn);
      if (previousNotes[c] != cn) {
        if (previousNotes[c] > 0) midifile.addNoteOff(0, t, 0, key(c), 0);
        if (cn > 0) midifile.addNoteOn(0, t, 0, key(c), cn);
      }
    }

    previousNotes = notes;
  }

  void processEnd (uint timestep) {
    std::vector<uchar> notesOff { 0xB0, 0x7B, 0x00 };
    midifile.addEvent(0, TPQ*timestep, notesOff);
  }

  void write (const stdfs::path &path) {

  //  for (uint c=0; c<C; c++)
  //    if (on[c])
  //      midifile.addNoteOff(0, tpq * N, 0, A+c, 0);
//    midifile.addEvent(0, TPQ * N, notesOff);

//    midifile.sortTracks();
    midifile.doTimeAnalysis();
    midifile.write(path);

//    midifile.get

#define GET(X) "\t" #X ": " << midifile.get##X() << "\n"
    std::cout << "Wrote " << path << ":\n"
              << GET(FileDurationInQuarters)
              << GET(FileDurationInSeconds)
              << GET(FileDurationInTicks)
              << GET(FileDurationInSeconds);
#undef GET
  }
};
#endif

long maybeSeed(const std::string& s) {
  if (s.empty())  return -2;
  if (std::find_if(
        s.begin(),
        s.end(),
        [](unsigned char c) { return !std::isdigit(c) && c != '-'; }
      ) != s.end()) return -2;

  char* p;
  long l = strtol(s.c_str(), &p, 10);
  return p? l : -2;
}

// To prevent missing linkages
auto &apoget_force_link = config::PTree::rsetSize;

int main(int argc, char *argv[]) {
  using ANNViewer = kgd::es_hyperneat::gui::ann::Viewer;

  // Can only do color tagging for modular ANN (in xy-projected 2d)
  using MANNViewer = kgd::es_hyperneat::gui::ann2d::Viewer;

  using Ind = simu::Evaluator::Ind;
  using utils::operator<<;

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::QUIET;

  std::string indFile, scenarioStr, scenarioArg;

  int startspeed = 1;
  bool autoquit = false;

  stdfs::path outputFolder = "tmp/lg-gui/";
  char overwrite = simu::Simulation::ABORT;

  std::string picture;
  int snapshots = -1;
  bool noRestore = false;

  static const std::vector<std::string> validSnapshotViews {
    "simu", /*"ann",*/ "mann", "io",
#ifdef WITH_MIDI
    "midi",
#endif
  };
  std::vector<std::string> snapshotViews;
  std::string background;

#ifdef WITH_MIDI
  uint midiInstrument = 123;
#endif

  std::string cppnRender = "";
  std::string annRender = ""; // no extension
  std::string annNeuralTags;
  bool annAggregateNeurons = false;

  int lesions = 0;

  int trace = -1;
  bool tag = false;

  cxxopts::Options options("Splinoids (lg-gui-evaluation)",
                           "2D graphical simulation of speaking splinoids");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))

    ("ind", "Splinoid genome", cxxopts::value(indFile))
    ("scenario", "Scenario name to evaluate", cxxopts::value(scenarioStr))
    ("scenario-arg", "Eventual argument for the requested scenario",
     cxxopts::value(scenarioArg))

    ("start", "Whether to start running immendiatly after initialisation"
              " (and optionally at which speed > 1)",
     cxxopts::value(startspeed)->implicit_value("1"))
    ("auto-quit", "Whether to automatically quit at the end of the scenario",
     cxxopts::value(autoquit)->implicit_value("true"))
    ("f,data-folder", "Folder under which to store the computational outputs",
     cxxopts::value(outputFolder))
    ("overwrite", "Action to take if the data folder is not empty: either "
                  "[a]bort or [p]urge",
     cxxopts::value(overwrite))

    ("picture", "Generate a picture of provided size",
     cxxopts::value(picture)->implicit_value("25"))
    ("snapshots", "Generate simu+ann snapshots of provided size (batch)",
     cxxopts::value(snapshots)->implicit_value("25"))
    ("snapshots-views",
      ((std::ostringstream&)
        (std::ostringstream()
          << "Specifies which views to render. Valid values: "
          << validSnapshotViews)).str(),
     cxxopts::value(snapshotViews))
#ifdef WITH_MIDI
    ("midi-instrument",
     utils::mergeToString("Specify instrument for midi output (default: ",
                          midiInstrument, ")"),
     cxxopts::value(midiInstrument))
#endif
    ("no-restore", "Don't restore interactive view options",
     cxxopts::value(noRestore)->implicit_value("true"))
    ("background", "Override color for the views background",
     cxxopts::value(background))

    ("trace", "Render trajectories through alpha-increasing traces",
     cxxopts::value(trace))

    ("cppn-render", "Export qt-based visualisation of the genotype's CPPN",
     cxxopts::value(cppnRender)->implicit_value("png"))

    ("ann-render", "Export qt-based visualisation of the phenotype's ANN",
     cxxopts::value(annRender)->implicit_value("png"))
    ("ann-tags", "Specifiy a collections of position -> tag for the ANN nodes",
     cxxopts::value(annNeuralTags))
    ("ann-aggregate", "Group neurons into behavioral modules",
     cxxopts::value(annAggregateNeurons)->implicit_value("true"))

    ("lesions", "Lesion type to apply (def: 0/none)", cxxopts::value(lesions))


    ("tags", "Tag items to facilitate reading",
     cxxopts::value(tag)->implicit_value("true"))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    using utils::operator<<;
    std::cout << options.help()
              << std::endl;
    return 0;
  }

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  if (snapshots > 0) {
    for (const auto &o: {"start","f"})
      if (result.count(o))
        std::cout << "Warning: option '" << o << " is ignored in snapshot mode"
                  << "\n";
  }

//  if (indFile.empty()) utils::Thrower("No splinoid genome provided");

  // ===========================================================================
  // == Qt setup

  QApplication a(argc, argv);
  setlocale(LC_NUMERIC,"C");

  QApplication::setOrganizationName("kgd-al");
  QApplication::setApplicationName("splinoids-visu");
  QApplication::setApplicationDisplayName("Splinoids (visu)");
  QApplication::setApplicationVersion("0.0.0");
  QApplication::setDesktopFileName(
    QApplication::organizationName() + "-" + QApplication::applicationName()
    + ".desktop");
  QSettings settings;

  // ===========================================================================
  // == Config setup

#define RESTORE(X, T) \
  config::Visualisation::X.overrideWith(settings.value("hack::" #X).value<T>()); \
  if (verbosity != Verbosity::QUIET) \
    std::cerr << "Restoring " << #X << ": " << config::Visualisation::X() \
              << "\n";

  if (!noRestore) {
    RESTORE(animateANN, bool)
    RESTORE(brainDeadSelection, BrainDead)
    RESTORE(selectionZoomFactor, float)
    RESTORE(drawVision, int)
    RESTORE(drawAudition, bool)
  }

#undef RESTORE

  if (stdfs::path(configFile).filename() == "Simulation.config") {
    config::Simulation::setupConfig(configFile, Verbosity::QUIET);
    config::Visualisation::setupConfig("auto", Verbosity::QUIET);

  } else
    config::Visualisation::setupConfig(configFile, Verbosity::QUIET);

//#ifndef NDEBUG
//  std::cerr << "Overriding config values\n";
//  config::Simulation::auditionRange.overrideWith(0);
//#endif

  config::Simulation::verbosity.overrideWith(0);
  if (verbosity != Verbosity::QUIET)
    config::Visualisation::printConfig(std::cout);


  // ===========================================================================
  // == Data load

  auto params = simu::Evaluator::Params::fromArgv(scenarioStr, scenarioArg);
  Ind ind = simu::Evaluator::fromJsonFile(indFile);

  // ===========================================================================
  // == CPPN Renderings

  if (!cppnRender.empty()) {
    kgd::es_hyperneat::gui::cppn::Viewer v;
    v.setGraph(ind.dna.brain.cppn);

    const auto doRender = [&v] (QPaintDevice *d, QRect trect) {
      QPainter painter (d);
      painter.setRenderHint(QPainter::Antialiasing, true);

      QRect srect = v.mapFromScene(v.sceneRect()).boundingRect();
      v.render(&painter, trect, srect);
    };

    stdfs::path p;
    if (!result.count("data-folder") == 0)  p = outputFolder / "cppn.png";
    else {
      p = indFile;
      p.replace_extension();
      p += "_cppn." + cppnRender;
    }
    QString qp = QString::fromStdString(p);

    if (cppnRender == "png") {
      QImage img (v.sceneRect().size().toSize(), QImage::Format_RGB32);
      img.fill(Qt::white);

      doRender(&img, img.rect());

      if (img.save(qp))
        std::cout << "Rendered subject cppn into " << p << "\n";
       else
        std::cerr << "Failed to render subject cppn into " << p << "\n";

    } else if (cppnRender == "pdf") {
      QPrinter printer (QPrinter::HighResolution);
      printer.setPageSize(QPageSize(v.sceneRect().size(), QPageSize::Point));
      printer.setPageMargins(QMarginsF(0, 0, 0, 0));
      printer.setOutputFileName(qp);

      doRender(&printer,
               printer.pageLayout().paintRectPixels(printer.resolution()));

      std::cout << "Generated " << p << "\n";
    }

    return 0;
  }

  // ===========================================================================
  // == Window/layout setup

  QMainWindow w;
  gui::StatsView *stats = new gui::StatsView;
  visu::GraphicSimulation simulation (w.statusBar(), stats);
  simu::Scenario scenario (simulation);

  gui::MainView *v = new gui::MainView (simulation, stats, w.menuBar());
  gui::GeneticManipulator *cs = v->characterSheet();

  QSplitter splitter;
  splitter.addWidget(v);
  splitter.addWidget(stats);
  w.setCentralWidget(&splitter);

  w.setWindowTitle("Main window");

  // ===========================================================================
  // == Simulation setup

  auto s_params = params.scenarioParams(0);
  s_params.genome = ind.dna;
  scenario.init(s_params);
  if (ind.infos == "rmute") scenario.muteReceiver();

//  if (!annNeuralTags.empty() /*&& snapshots == -1 && annRender.empty()*/) {
//    phenotype::ANN &ann = scenario.subject()->brain();
//    simu::Evaluator::applyNeuralFlags(ann, annNeuralTags);
//  }
//  if (lesions > 0)  scenario.applyLesions(lesions);

  if (!outputFolder.empty()) {
    bool ok = simulation.setWorkPath(outputFolder,
                                     simu::Simulation::Overwrite(overwrite));
    if (!ok)  exit(2);
  }

  // ===========================================================================
  // Final preparations (for all versions)

  v->fitInView(simulation.bounds(), Qt::KeepAspectRatio);
//  v->select(simulation.critters().at(scenario.receiver()));

  if (tag)
    for (auto p: simulation.critters())
      p.second->tag =
        QString(simu::Scenario::critterRole((p.first->id()))).toUpper();

//#ifndef NDEBUG
//  std::cerr << "Forcing debug view\n";
//  v->action("b2 debug draw")->trigger();
//  v->action("Stats")->trigger();
//#endif

  if (!background.empty()) {
    auto p = a.palette();
    QColor c = QColor(QString::fromStdString(background));
    if (c.isValid()) {
      p.setColor(QPalette::Base, c);
      a.setPalette(p);
    } else
      std::cerr << "WARNING: " << background
                << " is not a valid QColor specification. Ignoring\n";
  }

  int r = 242;  // return value

  if (!picture.empty()) {
//    auto teams = scenario.teams();
//    for (uint i=0; i<paths.size(); i++) {
//      stdfs::path obase = stdfs::path(paths[i]).replace_extension();
//      auto &t = teams[i];
//      bool atomic = (t.size() == 1);

//      uint j=0;
//      for (auto it = t.begin(); it != t.end(); ++it, j++) {
//        visu::Critter *c = simulation.critters().at(*it);
//        QString output = QString::fromStdString(obase);
//        if (!atomic)  output += "_" + QString::number(j);

//        if (picture == "pdf")
//              c->printPhenotypePdf(output + ".pdf");
//        else  c->printPhenotypePng(output + ".png", std::stoi(picture));
//      }
//    }

    std::cerr << "Nothing to render, right?\n";
    r = 0;

  } else if (snapshots > 0) {
  // ===========================================================================
  // Batch snapshot mode

    stdfs::path savefolder = stdfs::path(outputFolder) / "screenshots";
    stdfs::create_directories(savefolder);

    config::Visualisation::brainDeadSelection.overrideWith(BrainDead::UNSET);
//    config::Visualisation::drawVision.overrideWith(2);
    config::Visualisation::drawAudition.overrideWith(0);

//    config::Visualisation::printConfig(std::cout);

    if (snapshotViews.empty())  snapshotViews = validSnapshotViews;

    // Build list of views requested for rendering
    std::map<std::string, QWidget*> views;
    std::unique_ptr<phenotype::ModularANN> mann;
    std::unique_ptr<MANNViewer> mannViewer;
#ifdef WITH_MIDI
    std::vector<std::unique_ptr<MidiFileWrapper>> midis;
#else
    bool midis = false;
#endif

    for (const std::string &v_name: snapshotViews) {
      QWidget *view = nullptr;
      if (v_name == "simu") {
        view = v;
        v->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        v->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        QRectF b = simulation.bounds();
        view->setFixedSize(snapshots, snapshots * b.height() / b.width());

//      } else if (v_name == "ann") {
//        view = cs->brainPanel()->annViewerWidget;
//        view->setMinimumSize(snapshots, snapshots);

      } else if (v_name == "io") {
        view = cs->brainIO();
        view->setMaximumWidth(snapshots);

      } else if (v_name == "mann" && !annNeuralTags.empty()
               && annAggregateNeurons) {
        phenotype::ANN &ann = scenario.receiver()->brain();
        simu::Evaluator::applyNeuralFlags(ann, annNeuralTags);

        mann.reset(new phenotype::ModularANN(ann));
        mannViewer.reset(new MANNViewer);
        auto mav = mannViewer.get();
        mav->setGraph(*mann);
        mav->updateCustomColors();
        mav->startAnimation();

        view = mav;
        view->setMinimumSize(snapshots, snapshots);

#ifdef WITH_MIDI
      } else if (v_name == "midi") {
        for (uint i=0; i<2; i++)
          midis.push_back(std::make_unique<MidiFileWrapper>(midiInstrument));
#endif
      }

      if (view) views[v_name] = view;
    }

    // On each step, render all views
    const auto generate =
      [v, &simulation, &scenario, &savefolder, &views,
        &mann, &midis, snapshots] {
      v->focusOnSelection();
      v->characterSheet()->readCurrentStatus();

      auto t = simulation.currTime().timestamp();
      for (const auto &p: views) {
        if (p.first == "mann") {
          mann->update();
          static_cast<MANNViewer*>(p.second)->updateAnimation();
        }

        std::ostringstream oss;
        oss << savefolder.string() << "/" << std::setfill('0') << std::setw(5)
            << t << "_" << p.first << ".png";
        auto savepath = oss.str();
        auto qsavepath = QString::fromStdString(savepath);
        oss.str();

        oss << "Saving to " << savepath << ": ";
        bool ok = p.second->grab().save(qsavepath);

        oss << (ok ? "OK\n" : "FAILED\n");
        std::cout << std::setw(80) << oss.str();
        if (!ok) exit (1);
      }

#ifdef WITH_MIDI
      for (uint i=0; i<2; i++) {
        auto c = scenario.critters()[i];
        if (midis.size() <= i) continue;
        auto sounds = c->producedSound();
        MidiFileWrapper::Notes notes;
        for (uint j=0; j<notes.size(); j++) notes[j] = sounds[j+1];
        midis[i]->process(t, notes);
      }
#endif
    };

    std::cerr << "\n\nStarting snapshots generation\n";

    generate();
    std::cerr << "Generated\n";
    while (!simulation.finished()) {
      simulation.step();
      generate();
    }

#ifdef WITH_MIDI
    for (uint i=0; i<midis.size(); i++) {
      midis[i]->processEnd(simulation.currTime().timestamp());
      midis[i]->write(savefolder / utils::mergeToString(i, ".mid"));
    }
#endif

    r = 0;
//    std::cerr << "Batch snapshot mode incompatible with 3D ANN Viewer\n";

  } else if (trace >= 0) {
    config::Visualisation::trace.overrideWith(trace);

    config::Visualisation::brainDeadSelection.overrideWith(BrainDead::UNSET);
    v->select(v->selection());  // refresh selection (if any) brainDead flag

    config::Visualisation::drawVision.overrideWith(0);
    config::Visualisation::drawAudition.overrideWith(0);

    stdfs::path savepath = outputFolder / "ptrajectory.png";
    stdfs::create_directories(savepath.parent_path());

    while (!simulation.finished()) simulation.step();

    simulation.render(QString::fromStdString(savepath));
    std::cout << "Saved to: " << savepath << "\n";
    r=0;

  } else if (!annRender.empty() && !annNeuralTags.empty()
             && annAggregateNeurons) {

#if ESHN_SUBSTRATE_DIMENSION == 2
        cs->brainPanel()->annViewer->updateCustomColors();
#endif

    MANNViewer av;
    phenotype::ANN &ann = scenario.subject()->brain();
    simu::Evaluator::applyNeuralFlags(ann, annNeuralTags);

    phenotype::ModularANN annAgg (ann);
    av.setGraph(annAgg);
    av.updateCustomColors();

    const auto doRender = [&av] (QPaintDevice *d, QRect trect) {
      QPainter painter (d);
      painter.setRenderHint(QPainter::Antialiasing, true);

      QRect srect = av.mapFromScene(av.sceneRect()).boundingRect();
      trect.adjust(.5*(srect.height() - srect.width()), 0, 0, 0);
      av.render(&painter, trect, srect);
    };

    const auto render =
      [&av, doRender] (const std::string &path, const std::string &ext) {
      std::string fullPath = path + "." + ext;
      QString qPath = QString::fromStdString(fullPath);

      if (ext == "png") {
        QImage img (500, 500, QImage::Format_RGB32);
        img.fill(Qt::white);

        doRender(&img, img.rect());

        if (img.save(qPath))
          std::cout << "Rendered subject brain into " << fullPath << "\n";
         else
          std::cerr << "Failed to render subject brain into " << fullPath
                    << "\n";

      } else if (ext == "pdf") {
        QPrinter printer (QPrinter::HighResolution);
        printer.setPageSize(QPageSize(av.sceneRect().size(), QPageSize::Point));
        printer.setPageMargins(QMarginsF(0, 0, 0, 0));
        printer.setOutputFileName(qPath);

        doRender(&printer,
                 printer.pageLayout().paintRectPixels(printer.resolution()));
      }
    };

    render(stdfs::path(annNeuralTags).parent_path() / "mann", annRender);
    r = 0;

  } else {
  // ===========================================================================
  // Regular version
    w.restoreGeometry(settings.value("geom").toByteArray());
    cs->restoreGeometry(settings.value("cs::geom").toByteArray());
    cs->setVisible(settings.value("cs::visible").toBool());
    w.show();

    v->setAutoQuit(autoquit);
    v->fitInView(simulation.bounds(), Qt::KeepAspectRatio);

    QTimer::singleShot(100, [&v, startspeed] {
      if (startspeed) v->start(startspeed);
      else            v->stop();
    });

//    using Eval = simu::Evaluator;
//    auto log = Eval::logging_getData();
//    Eval::logging_init(log, stdfs::path(outputFolder), scenario);
//    QObject::connect(v, &gui::MainView::stepped,
//                     [log,&scenario] {
//      Eval::logging_step(log, scenario);
//    });

    r = a.exec();

    if (!params.neuralEvaluation())
      std::cout << "Score: " << scenario.score() << " ("
                << s_params.spec.name
                << ")\n";

//    Eval::logging_freeData(&log);

    settings.setValue("cs::visible", cs->isVisible());
    settings.setValue("cs::geom", cs->saveGeometry());
    settings.setValue("geom", w.saveGeometry());

  #define SAVE(X, T) \
    settings.setValue("hack::" #X, T(config::Visualisation::X())); \
    if (verbosity != Verbosity::QUIET) \
      std::cerr << "Saving " << #X << ": " << config::Visualisation::X() \
                << "\n";
    SAVE(animateANN, bool)
    SAVE(brainDeadSelection, int)
    SAVE(selectionZoomFactor, float)
    SAVE(drawVision, int)
    SAVE(drawAudition, bool)
  #undef SAVE
  }

  return r;
}
