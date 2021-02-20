#include <QApplication>
#include <QMainWindow>
#include <QSettings>
#include <QTimer>
#include <QSplitter>
#include <QDockWidget>

#include "kgd/external/cxxopts.hpp"

#include "scenario.h"
#include "../gui/mainview.h"

#include <QDebug>
#include "../visu/config.h"

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
  using EGenome = genotype::Environment;

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::QUIET;

  simu::Simulation::InitData idata = simu::Scenario::commonInitData;

  std::string eGenomeArg, cGenomeArg, scenarioArg;

  genotype::Environment eGenome;
  genotype::Critter cGenome;

  int startspeed = 1;

  std::string outputFolder = "tmp/pp-gui/";
  char overwrite = simu::Simulation::ABORT;

  int snapshots = -1;

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

    ("env-genome", "Environment's genome", cxxopts::value(eGenomeArg))
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

#define RESTORE(X, T) \
  config::Visualisation::X.ref() = settings.value("hack::" #X).value<T>(); \
  std::cerr << "Restoring " << #X << ": " << config::Visualisation::X() << "\n";
  RESTORE(brainDeadSelection, BrainDead)
  RESTORE(selectionZoomFactor, bool)
  RESTORE(drawVision, int)
  RESTORE(drawAudition, bool)

#undef RESTORE

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

  if (verbosity != Verbosity::QUIET) config::Simulation::printConfig(std::cout);
  if (configFile.empty()) config::Simulation::printConfig("");

  std::cout << "Reading environment genome from input file '"
            << eGenomeArg << "'" << std::endl;
  eGenome = EGenome::fromFile(eGenomeArg);

  std::cout << "Reading splinoid genome from input file '"
            << cGenomeArg << "'" << std::endl;
  cGenome = CGenome::fromFile(cGenomeArg);
  cGenome.gdata.self.gid = std::max(cGenome.gdata.self.gid,
                                    phylogeny::GID(0));

  simulation.init(eGenome, {}, idata);
  scenario.init(cGenome);

  if (!outputFolder.empty()) {
    bool ok = simulation.setWorkPath(outputFolder, simu::Simulation::Overwrite(overwrite));
    if (!ok)  exit(2);
  }

  // ===========================================================================
  // Final preparations (for regular version)
  if (snapshots <= 0) {
    w.restoreGeometry(settings.value("geom").toByteArray());
    cs->restoreGeometry(settings.value("cs::geom").toByteArray());
    cs->setVisible(settings.value("cs::visible").toBool());
    w.show();

    v->fitInView(simulation.bounds(), Qt::KeepAspectRatio);
    v->selectNext();

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
    SAVE(brainDeadSelection, int)
    SAVE(selectionZoomFactor, float)
    SAVE(drawVision, int)
    SAVE(drawAudition, bool)
  #undef SAVE

    return r;

  } else {
    stdfs::path savefolder = cGenomeArg;
    savefolder = savefolder.replace_extension();
    savefolder /= scenarioArg;
    stdfs::create_directories(savefolder);

    config::Visualisation::brainDeadSelection.overrideWith(BrainDead::UNSET);
    config::Visualisation::drawVision.overrideWith(2);
    config::Visualisation::drawAudition.overrideWith(0);

    v->fitInView(simulation.bounds(), Qt::KeepAspectRatio);
    v->select(simulation.critters().at(scenario.subject()));

    const auto generate = [v, &simulation, &scenario, &savefolder, snapshots] {
      v->focusOnSelection();
      v->characterSheet()->readCurrentStatus();

      int S = snapshots;
      QImage lhs (S, S, QImage::Format_RGB32),
             rhs (S, S, QImage::Format_RGB32),
             both (2*S, S, QImage::Format_RGB32);
      {
        lhs.fill(Qt::red);
        QPainter painter (&lhs);
        painter.setRenderHint(QPainter::Antialiasing, true);
        v->render(&painter, QRectF(),
                  v->mapFromScene(v->sceneRect()).boundingRect());

//        qDebug() << lhs.rect() << v->mapFromScene(v->sceneRect()).boundingRect();
      }
      {
        rhs.fill(Qt::white);
        QPainter painter (&rhs);
        painter.setRenderHint(QPainter::Antialiasing, true);
        v->characterSheet()->renderSubjectBrain(&painter);
      }
      {
        both.fill(Qt::blue);
        QPainter painter (&both);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.drawImage(0, 0, lhs);
        painter.drawImage(S, 0, rhs);
      }

      std::ostringstream oss;
      oss << savefolder.string() << "/" << std::setfill('0') << std::setw(5)
          << simulation.currTime().timestamp() << ".png";
      auto savepath = oss.str();
      auto qsavepath = QString::fromStdString(savepath);

      lhs.save(qsavepath + "-lhs.png");
      rhs.save(qsavepath + "-rhs.png");

      std::cout << "Saving to " << savepath << ": ";
      if (both.save(qsavepath))
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
  }

}
