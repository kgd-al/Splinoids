#include <QApplication>
#include <QMainWindow>
#include <QSettings>
#include <QTimer>
#include <QSplitter>
#include <QDockWidget>

#include "kgd/external/cxxopts.hpp"

#include "indevaluator.h"
#include "../gui/mainview.h"

#include <QDebug>
#include "../visu/config.h"
#include <QPrinter>

Q_DECLARE_METATYPE(BrainDead)

//#include "config/dependencies.h"

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

int main(int argc, char *argv[]) {
  // To prevent missing linkages
  std::cerr << config::PTree::rsetSize() << std::endl;
//  std::cerr << phylogeny::SID::INVALID << std::endl;

  using ANNViewer = kgd::es_hyperneat::gui::ann::Viewer;
  using CGenome = genotype::Critter;
  using Ind = simu::Evaluator::Ind;
  using utils::operator<<;

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::QUIET;

  std::string lhsTeamArg;
  std::string rhsArg;

  int startspeed = 1;
  bool autoquit = false;

  std::string outputFolder = "tmp/mk-gui/";
  char overwrite = simu::Simulation::ABORT;

  std::string picture;
  int snapshots = -1;
  bool noRestore = false;

  static const std::vector<std::string> validSnapshotViews {
    "simu", "ann", "mann", "io"
  };
  std::vector<std::string> snapshotViews;
  std::string background;

  std::string annRender = ""; // no extension
  std::string annNeuralTags;
  bool annAggregateNeurons = false;

  int lesions = 0;

  bool tag = false;
  int trace = -1;

  cxxopts::Options options("Splinoids (mk-gui-evaluation)",
                           "2D graphical simulation of simplified splinoids "
                           " for evolution in Mortal Kombat conditions");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))

    ("lhs", "Left-hand side team", cxxopts::value(lhsTeamArg))
    ("rhs", "Splinoid genomes for right-hand side team (for live evaluation)",
     cxxopts::value(rhsArg))
    ("scenario", "Scenario name for canonical evaluations",
     cxxopts::value(rhsArg))

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

    ("picture", "Generate a picture of provided size for both lhs & rhs",
     cxxopts::value(picture)->implicit_value("25"))
    ("snapshots", "Generate simu+ann snapshots of provided size (batch)",
     cxxopts::value(snapshots)->implicit_value("25"))
    ("snapshots-views",
      ((std::ostringstream&)
        (std::ostringstream()
          << "Specifies which views to render. Valid values: "
          << validSnapshotViews)).str(),
     cxxopts::value(snapshotViews))
    ("no-restore", "Don't restore interactive view options",
     cxxopts::value(noRestore)->implicit_value("true"))
    ("background", "Override color for the views background",
     cxxopts::value(background))

    ("trace", "Render trajectories through alpha-increasing traces",
     cxxopts::value(trace))

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
              << "\nCanonical scenarios:" << simu::Evaluator::canonicalScenarios
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

  if (lhsTeamArg.empty())
    utils::doThrow<std::invalid_argument>("No data provided for lhs team");

  if (rhsArg.empty())
    utils::doThrow<std::invalid_argument>(
      "No evaluation context provided. Either specify an opponent team (via rhs)"
      " or a canonical scenario (via scenario)");


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
    config::Visualisation::setupConfig("auto", Verbosity::QUIET);
    config::Simulation::setupConfig(configFile, Verbosity::QUIET);

  } else
    config::Visualisation::setupConfig(configFile, Verbosity::QUIET);


  config::Simulation::verbosity.overrideWith(0);
  if (verbosity != Verbosity::QUIET)
    config::Visualisation::printConfig(std::cout);


  // ===========================================================================
  // == Data load

  Ind lhsTeam = simu::Evaluator::fromJsonFile(lhsTeamArg);
  auto params = simu::Evaluator::fromString(lhsTeamArg, rhsArg);

  // ===========================================================================
  // == Window/layout setup

  QMainWindow w;
  gui::StatsView *stats = new gui::StatsView;
  visu::GraphicSimulation simulation (w.statusBar(), stats);
  simu::Scenario scenario (simulation, lhsTeam.dna.size());

  gui::MainView *v = new gui::MainView (simulation, stats, w.menuBar());
  gui::GeneticManipulator *cs = v->characterSheet();

  QSplitter splitter;
  splitter.addWidget(v);
  splitter.addWidget(stats);
  w.setCentralWidget(&splitter);

  w.setWindowTitle("Main window");

  // ===========================================================================
  // == Simulation setup

  scenario.init(lhsTeam.dna, params);
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
//  v->select(simulation.critters().at(scenario.subject()));
  if (tag) {
//    simulation.visuCritter(scenario.subject())->tag = "S";
//    if (auto c = scenario.clone())    simulation.visuCritter(c)->tag = "A";
//    if (auto p = scenario.predator()) simulation.visuCritter(p)->tag = "E";
//    if (auto f = scenario.foodlet())  simulation.visuFoodlet(f)->tag = "G";
  }

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
    bool atomic = (lhsTeam.dna.size() == 1);

    std::vector<std::string> paths { lhsTeamArg };
    if (!params.neuralEvaluation) paths.push_back(rhsArg);
    auto teams = scenario.teams();
    for (uint i=0; i<paths.size(); i++) {
      stdfs::path obase = stdfs::path(paths[i]).replace_extension();
      auto &t = teams[i];

      uint j=0;
      for (auto it = t.begin(); it != t.end(); ++it, j++) {
        visu::Critter *c = simulation.critters().at(*it);
        QString output = QString::fromStdString(obase);
        if (!atomic)  output += "_" + QString::number(j);

        if (picture == "pdf")
              c->printPhenotypePdf(output + ".pdf");
        else  c->printPhenotypePng(output + ".png", std::stoi(picture));
      }
    }

    r = 0;

  } else if (snapshots > 0) {
  // ===========================================================================
  // Batch snapshot mode

//    stdfs::path savefolder = cGenomeArg;
//    savefolder = savefolder.replace_extension()
//                / simu::IndEvaluator::specToString(scenario.specs(), lesions)
//                / "screenshots";
//    stdfs::create_directories(savefolder);

//    config::Visualisation::brainDeadSelection.overrideWith(BrainDead::UNSET);
////    config::Visualisation::drawVision.overrideWith(2);
//    config::Visualisation::drawAudition.overrideWith(0);

////    config::Visualisation::printConfig(std::cout);

//    if (snapshotViews.empty())  snapshotViews = validSnapshotViews;

//    // Build list of views requested for rendering
//    std::map<std::string, QWidget*> views;
//    std::unique_ptr<phenotype::ModularANN> mann;
//    for (const std::string &v_name: snapshotViews) {
//      QWidget *view = nullptr;
//      if (v_name == "simu") {
//        view = v;
//        v->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
//        v->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

//        QRectF b = simulation.bounds();
//        view->setFixedSize(snapshots, snapshots * b.height() / b.width());

//      } else if (v_name == "ann") {
//        view = cs->brainPanel()->annViewerWidget;
//        view->setMinimumSize(snapshots, snapshots);

//      } else if (v_name == "io") {
//        view = cs->brainIO();
//        view->setMaximumWidth(snapshots);

//      } else if (v_name == "mann" && !annNeuralTags.empty()
//               && annAggregateNeurons) {
//        phenotype::ANN &ann = scenario.subject()->brain();
//        simu::IndEvaluator::applyNeuralFlags(ann, annNeuralTags);

//        ANNViewer *av = cs->brainPanel()->annViewer;
//        av->updateCustomColors();

//        // Also aggregate similar inputs
//        for (auto &p: ann.neurons()) {
//          phenotype::Point pos = p->pos;
//          phenotype::ANN::Neuron &n = *p;
//          if (n.type == phenotype::ANN::Neuron::I)
//            n.flags = (pos.x() < 0)<<1 | (pos.y() < -.75)<<2 | 1<<3;
//        }

//        mann.reset(new phenotype::ModularANN(ann));
//        auto mav = new ANNViewer;
//        mav->setGraph(*mann);
//        mav->updateCustomColors();
//        mav->startAnimation();

//        view = mav;
//        view->setMinimumSize(snapshots, snapshots);
//      }

//      if (view) views[v_name] = view;
//    }

//    // On each step, render all views
//    const auto generate =
//      [v, &simulation, &savefolder, &views, &mann, snapshots] {
//      v->focusOnSelection();
//      v->characterSheet()->readCurrentStatus();

//      for (const auto &p: views) {
//        if (p.first == "mann") {
//          mann->update();
//          static_cast<ANNViewer*>(p.second)->updateAnimation();
//        }

//        std::ostringstream oss;
//        oss << savefolder.string() << "/" << std::setfill('0') << std::setw(5)
//            << simulation.currTime().timestamp() << "_" << p.first << ".png";
//        auto savepath = oss.str();
//        auto qsavepath = QString::fromStdString(savepath);

//        std::cout << "Saving to " << savepath << ": ";
//        if (p.second->grab().save(qsavepath))
//          std::cout << "OK     \r";
//        else {
//          std::cout << "FAILED !";
//          exit (1);
//        }
//      }
//    };

//    generate();
//    while (!simulation.finished()) {
//      simulation.step();
//      generate();
//    }

//    r = 0;
    std::cerr << "Batch snapshot mode incompatible with 3D ANN Viewer\n";

  } else if (trace > 0) {
//    config::Visualisation::trace.overrideWith(trace);

//    config::Visualisation::brainDeadSelection.overrideWith(BrainDead::UNSET);
//    config::Visualisation::drawVision.overrideWith(0);
//    config::Visualisation::drawAudition.overrideWith(0);

//    stdfs::path savefolder = cGenomeArg;
//    stdfs::path savepath = savefolder.replace_extension();
//    savepath /= scenarioArg;
//    if (lesions > 0) {
//      std::ostringstream oss;
//      oss << "_l" << lesions;
//      savepath += oss.str();
//    }
//    savepath /= "ptrajectory.png";
//    stdfs::create_directories(savepath.parent_path());

//    while (!simulation.finished())  simulation.step();

//    simulation.render(QString::fromStdString(savepath));
//    std::cout << "Saved to: " << savepath << "\n";
    std::cerr << "Trace mode incompatible with NxN kombats\n";

  } else if (!annRender.empty()) {
//    ANNViewer *av = cs->brainPanel()->annViewer;
//    av->stopAnimation();

//    const auto doRender = [av] (QPaintDevice *d, QRectF trect) {
//      QPainter painter (d);
//      painter.setRenderHint(QPainter::Antialiasing, true);

//      QRect srect = av->mapFromScene(av->sceneRect()).boundingRect();
//      trect.adjust(.5*(srect.height() - srect.width()), 0, 0, 0);
//      av->render(&painter, trect, srect);
//    };

//    const auto render =
//      [av, doRender] (const std::string &path, const std::string &ext) {
//      std::string fullPath = path + "." + ext;
//      QString qPath = QString::fromStdString(fullPath);

//      if (ext == "png") {
//        QImage img (500, 500, QImage::Format_RGB32);
//        img.fill(Qt::white);

//        doRender(&img, img.rect());

//        if (img.save(qPath))
//          std::cout << "Rendered subject brain into " << fullPath << "\n";
//         else
//          std::cerr << "Failed to render subject brain into " << fullPath
//                    << "\n";

//      } else if (ext == "pdf") {
//        QPrinter printer (QPrinter::HighResolution);
//        printer.setPageSize(QPageSize(av->sceneRect().size(), QPageSize::Point));
//        printer.setPageMargins(QMarginsF(0, 0, 0, 0));
//        printer.setOutputFileName(qPath);

//        doRender(&printer,
//                 printer.pageLayout().paintRectPixels(printer.resolution()));
//      }
//    };

//    if (annNeuralTags.empty())
//      render(stdfs::path(cGenomeArg).replace_extension() / "ann", annRender);

//    else {
//      phenotype::ANN &ann = scenario.subject()->brain();
//      simu::IndEvaluator::applyNeuralFlags(ann, annNeuralTags);

//      av->updateCustomColors();
//      render(stdfs::path(annNeuralTags).parent_path() / "ann_colored",
//             annRender);

//      if (annAggregateNeurons) {
//        // Also aggregate similar inputs
//        for (auto &p: ann.neurons()) {
//          phenotype::Point pos = p->pos;
//          phenotype::ANN::Neuron &n = *p;
//          if (n.type == phenotype::ANN::Neuron::I)
//            n.flags = (pos.x() < 0)<<1 | (pos.y() < -.75)<<2 | 1<<3;
//        }

//        phenotype::ModularANN annAgg (ann);
//        av->setGraph(annAgg);
//        av->updateCustomColors();
//        render(stdfs::path(annNeuralTags).parent_path() / "ann_clustered",
//               annRender);
//      }
//    }
    std::cerr << "ANN Render mode incompatible with 3D ANN Viewer\n";

  } else {
  // ===========================================================================
  // Regular version
    w.restoreGeometry(settings.value("geom").toByteArray());
    cs->restoreGeometry(settings.value("cs::geom").toByteArray());
    cs->setVisible(settings.value("cs::visible").toBool());
    w.show();

    v->setAutoQuit(autoquit);
    v->fitInView(simulation.bounds(), Qt::KeepAspectRatio);
    v->select(nullptr);

    QTimer::singleShot(100, [&v, startspeed] {
      if (startspeed) v->start(startspeed);
      else            v->stop();
    });

    using Eval = simu::Evaluator;
    auto log = Eval::logging_getData();
    Eval::logging_init(log, stdfs::path(outputFolder) /  params.kombatName,
                       scenario);
    QObject::connect(v, &gui::MainView::stepped,
                     [log,&scenario] {
      Eval::logging_step(log, scenario);
    });

    r = a.exec();

    Eval::logging_freeData(&log);

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

  std::cout << "Score: " << scenario.score() << "\n";

  return r;
}
