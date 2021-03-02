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

  using CGenome = genotype::Critter;

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
  bool withTrace = false; /// TODO not used

  std::string annRender = ""; // no extension
  std::string annRenderingColors;
  float annAlphaThreshold = 0;

  bool v1scenarios = false;
  int lesions = 0;

  bool tag = false;

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
    ("with-trace", "Splinoids leave a trace of their trajectory",
     cxxopts::value(withTrace)->implicit_value("-1"))

    ("ann-render", "Export qt-based visualisation of the phenotype's ANN",
     cxxopts::value(annRender)->implicit_value("png"))
    ("ann-colors", "Specifiy a collections of positions/colors for the ANN's"
     " nodes", cxxopts::value(annRenderingColors))
    ("ann-threshold", "Rendering threshold for provided alpha value"
                      " (inclusive)", cxxopts::value(annAlphaThreshold))

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
      utils::doThrow<std::invalid_argument>(
        "Provided splinoid genome file '", cGenomeArg, " does not exist!");
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
  RESTORE(animateANN, bool)
  RESTORE(brainDeadSelection, BrainDead)
  RESTORE(selectionZoomFactor, bool)
  RESTORE(drawVision, int)
  RESTORE(drawAudition, bool)

#undef RESTORE

  if (verbosity != Verbosity::QUIET) config::Simulation::printConfig(std::cout);
  config::Simulation::verbosity.overrideWith(0);
  if (configFile.empty()) config::Simulation::printConfig("");

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

  scenario.init(cGenome, lesions);

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
    if (auto c = scenario.clone()) simulation.visuCritter(c)->tag = "A";
    if (auto p = scenario.predator()) simulation.visuCritter(p)->tag = "E";
    simulation.visuFoodlet(scenario.foodlet())->tag = "G";
  }


  if (snapshots > 0) {
  // ===========================================================================
  // Batch snapshot mode

    stdfs::path savefolder = cGenomeArg;
    savefolder = savefolder.replace_extension() / scenarioArg / "screenshots";
    stdfs::create_directories(savefolder);

    config::Visualisation::brainDeadSelection.overrideWith(BrainDead::UNSET);
    config::Visualisation::drawVision.overrideWith(2);
    config::Visualisation::drawAudition.overrideWith(0);

    // Create ad-hock widget for rendering
    QWidget *renderer = new QWidget;
    renderer->setContentsMargins(0, 0, 0, 0);
    QGridLayout *layout = new QGridLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(v, 0, 0, 2, 1);
    layout->addWidget(cs->brainIO(), 0, 1);
    layout->addWidget(cs->brainPanel()->annViewer, 1, 1);
    renderer->setLayout(layout);

    const auto generate = [v, &simulation, &savefolder, renderer, snapshots] {
      v->focusOnSelection();
      v->characterSheet()->readCurrentStatus();

      std::ostringstream oss;
      oss << savefolder.string() << "/" << std::setfill('0') << std::setw(5)
          << simulation.currTime().timestamp() << ".png";
      auto savepath = oss.str();
      auto qsavepath = QString::fromStdString(savepath);

      std::cout << "Saving to " << savepath << ": ";
      if (renderer->grab().scaledToHeight(snapshots, Qt::SmoothTransformation)
          .save(qsavepath))
        std::cout << "OK\r";
      else {
        std::cout << "FAILED !";
        exit (1);
      }
    };

    generate();
    while (!simulation.finished()) {
      simulation.step();
      generate();
    }

    return 0;

  } else if (!annRender.empty()) {
    using ANNViewer = kgd::es_hyperneat::gui::ann::Viewer;
    ANNViewer *av = cs->brainPanel()->annViewer;
    av->stopAnimation();


//    {

//      using ANNViewer = kgd::es_hyperneat::gui::ann::Viewer;
//      ANNViewer *av = cs->brainPanel()->annViewer;
//      av->stopAnimation();


//      QPainter painter (&printer);

//      QRectF trect = img.rect();
//      QRect srect = av->mapFromScene(av->sceneRect()).boundingRect();
//      trect.adjust(.5*(srect.height() - srect.width()), 0, 0, 0);
//      av->render(&painter, trect, srect);

//      exit(42);
//    }
    const auto doRender = [av] (QPaintDevice *d, QRectF trect) {
      QPainter painter (d);
      painter.setRenderHint(QPainter::Antialiasing, true);

      QRect srect = av->mapFromScene(av->sceneRect()).boundingRect();
      trect.adjust(.5*(srect.height() - srect.width()), 0, 0, 0);
      av->render(&painter, trect, srect);
    };

    const auto render =
      [av, doRender] (const std::string &path, const std::string &ext) {
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
        printer.setPageSize(QPageSize(av->sceneRect().size(), QPageSize::Point));
        printer.setPageMargins(QMarginsF(0, 0, 0, 0));
        printer.setOutputFileName(qPath);

        doRender(&printer, printer.pageRect());
      }
    };

    if (annRenderingColors.empty())
      render(stdfs::path(cGenomeArg).replace_extension() / "ann", annRender);

    else {
      ANNViewer::CustomNodeColors colors;
      std::ifstream ifs (annRenderingColors);
      if (!ifs)
        utils::doThrow<std::invalid_argument>(
          "Failed to open color mapping '", annRenderingColors, "'");

      for (std::string line; std::getline(ifs, line); ) {
        if (line.empty() || line[0] == '/') continue;
        std::istringstream iss (line);
        ANNViewer::CustomNodeColors::key_type pos;
        iss >> pos;

        ANNViewer::CustomNodeColors::mapped_type list;
        std::string color;
        while (iss >> color) {
          QColor c (QString::fromStdString(color));
          if (c.isValid() && c.alphaF() >= annAlphaThreshold)
            list.push_back(c);
        }

        if (!list.isEmpty())  colors[pos] = list;
      }

      if (verbosity != Verbosity::QUIET) {
        std::cout << "Parsed color map:\n";
        for (const auto &p: colors) {
          std::cout << "\t" << std::setfill(' ') << std::setw(10) << p.first
                    << ":\t";
          for (const QColor &c: p.second)
            std::cout << " #" << std::hex << std::setfill('0') << std::setw(8)
                      << c.rgba() << std::dec;
          std::cout << "\n";
        }
      }

      av->setCustomColors(colors);
      render(stdfs::path(annRenderingColors).parent_path()
             / "ann_clustered", annRender);
    }

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
    std::cerr << "Saving " << #X << ": " << config::Visualisation::X() << "\n";
    SAVE(animateANN, bool)
    SAVE(brainDeadSelection, int)
    SAVE(selectionZoomFactor, float)
    SAVE(drawVision, int)
    SAVE(drawAudition, bool)
  #undef SAVE

    return r;

  }

}
