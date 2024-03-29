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

  using CGenome = genotype::Critter;
  using utils::operator<<;

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::QUIET;

  std::string cGenomeArg, scenarioArg = "10+";

  CGenome cGenome;

  int startspeed = 1;

  std::string outputFolder = "tmp/pp-gui/";
  char overwrite = simu::Simulation::ABORT;

  int snapshots = -1;
  bool noRestore = false;

  static const std::vector<std::string> validSnapshotViews {
    "simu", "ann", "mann", "io"
  };
  std::vector<std::string> snapshotViews;
  std::string background;

  std::string annRender = ""; // no extension
  std::string annNeuralTags;
  float annAlphaThreshold = 0;
  bool annAggregateNeurons = false;

  bool v1scenarios = false;
  int lesions = 0;

  bool tag = false;
  int trace = -1;

  cxxopts::Options options("Splinoids (pp-gui-evaluation)",
                           "2D simulation of minimal splinoids for the "
                           "evolution or carefulness and deception detection");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))

    ("spln-genome", "Splinoid genome", cxxopts::value(cGenomeArg))
    ("scenario", "Scenario specifications", cxxopts::value(scenarioArg))

    ("start", "Whether to start running immendiatly after initialisation"
              " (and optionally at which speed > 1)",
     cxxopts::value(startspeed)->implicit_value("1"))
    ("f,data-folder", "Folder under which to store the computational outputs",
     cxxopts::value(outputFolder))
    ("overwrite", "Action to take if the data folder is not empty: either "
                  "[a]bort or [p]urge",
     cxxopts::value(overwrite))

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
    ("ann-threshold", "Rendering threshold for provided alpha value"
                      " (inclusive)", cxxopts::value(annAlphaThreshold))
    ("ann-aggregate", "Group neurons into behavioral modules",
     cxxopts::value(annAggregateNeurons)->implicit_value("true"))

    ("1,v1", "Use v1 scenarios",
     cxxopts::value(v1scenarios)->implicit_value("true"))
    ("lesions", "Lesion type to apply (def: 0/none)", cxxopts::value(lesions))

    ("tags", "Tag items to facilitate reading",
     cxxopts::value(tag)->implicit_value("true"))

    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    return 0;
  }

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  if (snapshots > 0) {
    for (const auto &o: {"start","f"})
      if (result.count(o))
        std::cout << "Warning: option '" << o << " is ignored in snapshot mode"
                  << "\n";
    if (!stdfs::exists(cGenomeArg))
      utils::Thrower("Provided splinoid genome file '", cGenomeArg,
                     " does not exist!");
  }

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

//    config::Simulation::setupConfig(configFile, verbosity);

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

  if (verbosity != Verbosity::QUIET) config::Visualisation::printConfig(std::cout);
  config::Simulation::verbosity.overrideWith(0);
  if (configFile.empty()) config::Visualisation::printConfig("");

  // ===========================================================================
  // == Window/layout setup

  QMainWindow w;
  gui::StatsView *stats = new gui::StatsView;
  visu::GraphicSimulation simulation (w.statusBar(), stats);
  simu::Scenario scenario (scenarioArg, simulation);

  gui::MainView *v = new gui::MainView (simulation, stats, w.menuBar());
  gui::GeneticManipulator *cs = v->characterSheet();

  QSplitter splitter;
  splitter.addWidget(v);
  splitter.addWidget(stats);
  w.setCentralWidget(&splitter);

  w.setWindowTitle("Main window");

  // ===========================================================================
  // == Simulation setup

  std::cout << "Reading splinoid genome from input file '"
            << cGenomeArg << "'" << std::endl;
  cGenome = simu::IndEvaluator::fromJsonFile(cGenomeArg).dna;
  cGenome.gdata.self.gid = std::max(cGenome.gdata.self.gid,
                                    phylogeny::GID(0));

  scenario.init(cGenome);
  if (!annNeuralTags.empty() /*&& snapshots == -1 && annRender.empty()*/) {
    phenotype::ANN &ann = scenario.subject()->brain();
    simu::IndEvaluator::applyNeuralFlags(ann, annNeuralTags);
  }
  if (lesions > 0)  scenario.applyLesions(lesions);

  if (!outputFolder.empty()) {
    bool ok = simulation.setWorkPath(outputFolder,
                                     simu::Simulation::Overwrite(overwrite));
    if (!ok)  exit(2);
  }

  // ===========================================================================
  // Final preparations (for all versions)

  v->fitInView(simulation.bounds(), Qt::KeepAspectRatio);
  v->select(simulation.critters().at(scenario.subject()));
  if (tag) {
    simulation.visuCritter(scenario.subject())->tag = "S";
    if (auto c = scenario.clone())    simulation.visuCritter(c)->tag = "A";
    if (auto p = scenario.predator()) simulation.visuCritter(p)->tag = "E";
    if (auto f = scenario.foodlet())  simulation.visuFoodlet(f)->tag = "G";
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


  if (snapshots > 0) {
  // ===========================================================================
  // Batch snapshot mode

    stdfs::path savefolder = cGenomeArg;
    savefolder = savefolder.replace_extension()
                / simu::IndEvaluator::specToString(scenario.specs(), lesions)
                / "screenshots";
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
    for (const std::string &v_name: snapshotViews) {
      QWidget *view = nullptr;
      if (v_name == "simu") {
        view = v;
        v->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        v->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        QRectF b = simulation.bounds();
        view->setFixedSize(snapshots, snapshots * b.height() / b.width());

      } else if (v_name == "ann") {
        view = cs->brainPanel()->annViewerWidget;
        view->setMinimumSize(snapshots, snapshots);

      } else if (v_name == "io") {
        view = cs->brainIO();
        view->setMaximumWidth(snapshots);

      } else if (v_name == "mann" && !annNeuralTags.empty()
               && annAggregateNeurons) {
        phenotype::ANN &ann = scenario.subject()->brain();
        simu::IndEvaluator::applyNeuralFlags(ann, annNeuralTags);

        // Also aggregate similar inputs
        for (auto &p: ann.neurons()) {
          phenotype::Point pos = p->pos;
          phenotype::ANN::Neuron &n = *p;
          if (n.type == phenotype::ANN::Neuron::I)
            n.flags = (pos.x() < 0)<<1 | (pos.y() < -.75)<<2 | 1<<3;
        }

#if ESHN_SUBSTRATE_DIMENSION == 2
        cs->brainPanel()->annViewer->updateCustomColors();
#endif

        mann.reset(new phenotype::ModularANN(ann));
        mannViewer.reset(new MANNViewer);
        auto mav = mannViewer.get();
        mav->setGraph(*mann);
        mav->updateCustomColors();
        mav->startAnimation();

        view = mav;
        view->setMinimumSize(snapshots, snapshots);

      }

      if (view) views[v_name] = view;
    }

    // On each step, render all views
    const auto generate =
      [v, &simulation, &savefolder, &views, &mann, snapshots] {
      v->focusOnSelection();
      v->characterSheet()->readCurrentStatus();

      for (const auto &p: views) {
        if (p.first == "mann") {
          mann->update();
          static_cast<MANNViewer*>(p.second)->updateAnimation();
        }

        std::ostringstream oss;
        oss << savefolder.string() << "/" << std::setfill('0') << std::setw(5)
            << simulation.currTime().timestamp() << "_" << p.first << ".png";
        auto savepath = oss.str();
        auto qsavepath = QString::fromStdString(savepath);

        std::cout << "Saving to " << savepath << ": ";
        if (p.second->grab()/*.scaledToHeight(snapshots, Qt::SmoothTransformation)*/
            .save(qsavepath))
          std::cout << "OK     \r";
        else {
          std::cout << "FAILED !";
          exit (1);
        }
      }
    };

    generate();
    while (!simulation.finished()) {
      simulation.step();
      generate();
    }

    return 0;

  } else if (trace > 0) {
    config::Visualisation::trace.overrideWith(trace);

    config::Visualisation::brainDeadSelection.overrideWith(BrainDead::UNSET);
    config::Visualisation::drawVision.overrideWith(0);
    config::Visualisation::drawAudition.overrideWith(0);

    stdfs::path savefolder = cGenomeArg;
    stdfs::path savepath = savefolder.replace_extension();
    savepath /= scenarioArg;
    if (lesions > 0) {
      std::ostringstream oss;
      oss << "_l" << lesions;
      savepath += oss.str();
    }
    savepath /= "ptrajectory.png";
    stdfs::create_directories(savepath.parent_path());

    while (!simulation.finished())  simulation.step();

    simulation.render(QString::fromStdString(savepath));
    std::cout << "Saved to: " << savepath << "\n";

  } else if (!annRender.empty()) {
    std::cerr << "ANN rendering disabled\n";
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

  } else {
  // ===========================================================================
  // Regular version
    w.restoreGeometry(settings.value("geom").toByteArray());
    cs->restoreGeometry(settings.value("cs::geom").toByteArray());
    cs->setVisible(settings.value("cs::visible").toBool());
    w.show();

    v->fitInView(simulation.bounds(), Qt::KeepAspectRatio);
    v->select(simulation.critters().at(scenario.subject()));

    QTimer::singleShot(100, [&v, startspeed] {
      if (startspeed) v->start(startspeed);
      else            v->stop();
    });

    auto r = a.exec();

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

    return r;

  }

}
