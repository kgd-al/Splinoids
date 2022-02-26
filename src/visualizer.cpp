#include <QApplication>
#include <QMainWindow>
#include <QSettings>
#include <QTimer>
#include <QSplitter>
#include <QDockWidget>

#include "kgd/external/cxxopts.hpp"

#include "gui/mainview.h"

#include <QDebug>
#include "visu/config.h"

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

  simu::Simulation::InitData idata {};
  idata.ienergy = 1000;
  idata.cRatio = 1. / (1+config::Simulation::plantEnergyDensity());
  idata.nCritters = 50;
  idata.cRange = .25;
  idata.pRange = 1;
  idata.seed = -1;
  idata.cAge = 0;

  int startspeed = 1;

  int taurus = -1;
  std::string eGenomeArg = "-1";
  std::vector<std::string> cGenomeArgs;

  genotype::Environment eGenome;
  std::vector<genotype::Critter> cGenomes;
  uint cGenomeMutations = 0;

  std::string loadSaveFile, loadConstraints, loadFields;

//  std::string morphologiesSaveFolder, screenshotSaveFile;

//  int speed = 0;
//  bool autoQuit = false;

//  std::string duration = "=100";
  std::string outputFolder = "tmp_visu_run/";
  char overwrite = simu::Simulation::ABORT;

  cxxopts::Options options("Splinoids (visualisation)",
                           "2D simulation of critters in a changing environment");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))

    ("e,energy", "Total energy of the system", cxxopts::value(idata.ienergy))
    ("s,seed", "Seed value for simulation's RNG", cxxopts::value(idata.seed))
    ("n,i-splinoids", "Initial number of splinoids",
     cxxopts::value(idata.nCritters))
    ("i-prange", "Region in which initial plants are placed",
     cxxopts::value(idata.pRange))
    ("i-crange", "Region in which initial splinoids are placed",
     cxxopts::value(idata.cRange))
    ("i-sratio", "Initial fraction of energy devoted to the splinoids",
     cxxopts::value(idata.cRatio))
    ("i-age", "Initial age of splinoids population",
     cxxopts::value(idata.cAge))

    ("start", "Whether to start running immendiatly after initialisation"
              " (and optionally at which speed > 1)",
     cxxopts::value(startspeed)->implicit_value("1"))

    ("env-genome", "Environment's genome or a random seed",
     cxxopts::value(eGenomeArg))
    ("spln-genome", "Splinoid genome to start from or a random seed. Use "
                    "multiple times to define multiple sub-populations",
     cxxopts::value(cGenomeArgs))
    ("spln-mutations", "Additional mutations applied to the original genome",
     cxxopts::value(cGenomeMutations))

    ("taurus", "Whether the environment is a taurus or uses fixed boundaries",
      cxxopts::value(taurus))

//    ("d,duration", "Simulation duration. ",
//     cxxopts::value(duration))
    ("f,data-folder", "Folder under which to store the computational outputs",
     cxxopts::value(outputFolder))
    ("overwrite", "Action to take if the data folder is not empty: either "
                  "[a]bort or [p]urge",
     cxxopts::value(overwrite))
    ("l,load", "Load a previously saved simulation",
     cxxopts::value(loadSaveFile))
//    ("load-constraints", "Constraints to apply on dependencies check",
//     cxxopts::value(loadConstraints))
//    ("load-fields", "Individual fields to load",
//     cxxopts::value(loadFields))
//    ("q,auto-quit", "Quit as soon as the simulation ends",
//     cxxopts::value(autoQuit))
//    ("collect-morphologies", "Save morphologies in the provided folder",
//     cxxopts::value(morphologiesSaveFolder))
//    ("screenshot", "Save simulation state after initialisation/loading in"
//                   "specified file",
//     cxxopts::value(screenshotSaveFile))
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
//              << "\n\nOption 'auto-config' is the default and overrides 'config'"
//                 " if both are provided"
//              << "\nEither both 'plant' and 'environment' options are used or "
//                 "a valid file is to be provided to 'load' (the former has "
//                 "precedance in case all three options are specified)"
//              << "\n\n" << config::Dependencies::Help{}
//              << "\n\n" << simu::Simulation::LoadHelp{}
              << std::endl;
    return 0;
  }

//  bool missingArgument = (!result.count("environment") || !result.count("plant"))
//      && !result.count("load");

//  if (missingArgument) {
//    if (result.count("environment"))
//      utils::Thrower("No value provided for the plant's genome");

//    else if (result.count("plant"))
//      utils::Thrower("No value provided for the environment's genome");

//    else
//      utils::Thrower(
//        "No starting state provided. Either provide both an environment and plant genomes"
//        " or load a previous simulation");
//  }

////  if (!morphologiesSaveFolder.empty() && loadSaveFile.empty())
////    utils::Thrower(
////        "Generating morphologies is only meaningful when loading a simulation");

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

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
#ifndef NDEBUG
  RESTORE(b2DebugDraw, bool)
#endif
#undef RESTORE

  QMainWindow w;
  gui::StatsView *stats = new gui::StatsView;
  visu::GraphicSimulation s (w.statusBar(), stats);

  gui::MainView *v = new gui::MainView (s, stats, w.menuBar());
  gui::GeneticManipulator *cs = v->characterSheet();

  QSplitter splitter;
  splitter.addWidget(v);
  splitter.addWidget(stats);
  w.setCentralWidget(&splitter);

  w.setWindowTitle("Main window");

  // ===========================================================================
  // == Simulation setup

  if (loadSaveFile.empty()) {
//    config::Simulation::setupConfig(configFile, verbosity);
    if (verbosity != Verbosity::QUIET) config::Simulation::printConfig(std::cout);
    if (configFile.empty()) config::Simulation::printConfig("");
    genotype::Critter::printMutationRates(std::cout, 2);

    long eGenomeSeed = maybeSeed(eGenomeArg);
    if (eGenomeSeed < -1) {
      std::cout << "Reading environment genome from input file '"
                << eGenomeArg << "'" << std::endl;
      eGenome = EGenome::fromFile(eGenomeArg);

    } else {
      rng::FastDice dice;
      if (eGenomeSeed >= 0) dice.reset(eGenomeSeed);
      std::cout << "Generating environment genome from rng seed "
                << dice.getSeed() << std::endl;
      eGenome = EGenome::random(dice);
    }
    if (taurus != -1) eGenome.taurus = taurus;
    eGenome.toFile("last", 2);


    if (cGenomeArgs.empty())  cGenomeArgs.push_back("-1");
    for (const auto arg: cGenomeArgs) {
      long cGenomeSeed = maybeSeed(arg);
      if (cGenomeSeed < -1) {
        std::cout << "Reading splinoid genome from input file '"
                  << arg << "'" << std::endl;
        cGenomes.push_back(CGenome::fromFile(arg));

      } else {
        rng::FastDice dice;
        if (cGenomeSeed >= 0) dice.reset(cGenomeSeed);
        std::cout << "Generating splinoid genome from rng seed "
                  << dice.getSeed() << std::endl;
        CGenome g = CGenome::random(dice);
        for (uint i=0; i<cGenomeMutations; i++) g.mutate(dice);
        cGenomes.push_back(g);
      }
    }

//      cGenome.toFile("last", 2);

    if (verbosity > config::Verbosity::QUIET) {
      std::cout << "Environment:\n" << eGenome
                << "\nSplinoid";
      if (cGenomes.size() > 1) std::cout << "s";
      std::cout << ":\n";
      for (const CGenome &g: cGenomes)
        std::cout << g;
      std::cout << std::endl;
    }

    s.init(eGenome, cGenomes, idata);

  } else {
    visu::GraphicSimulation::load(loadSaveFile, s, loadConstraints, loadFields);
    if (verbosity != Verbosity::QUIET)
      config::Simulation::printConfig();
  }

  if (!outputFolder.empty()) {
    bool ok = s.setWorkPath(outputFolder, simu::Simulation::Overwrite(overwrite));
    if (!ok)  exit(2);
  }

//  if (!duration.empty()) {
//    if (duration.size() < 2)
//      utils::Thrower(
//        "Invalid duration '", duration, "'. [+|=]<years>");

//    uint dvalue = 0;
//    if (!(std::istringstream (duration.substr(1)) >> dvalue))
//      utils::Thrower(
//        "Failed to parse '", duration.substr(1), "' as a duration (uint)");

//    s.setDuration(simu::Environment::DurationSetType(duration[0]), dvalue);
//  }

//  int ret = 0;
//  if (!morphologiesSaveFolder.empty()) {
//    if (!stdfs::exists(morphologiesSaveFolder)) {
//      stdfs::create_directories(morphologiesSaveFolder);
//      std::cout << "Created folder(s) " << morphologiesSaveFolder
//                << " for morphologies storage" << std::endl;
//    }

//    // If just initialised, let it have one step to show its morphology
//    if (loadSaveFile.empty())
//      c.step();

//    v->saveMorphologies(QString::fromStdString(morphologiesSaveFolder));

//  } else if (!screenshotSaveFile.empty()) {
//    auto img = v->fullScreenShot();
//    img.save(QString::fromStdString(screenshotSaveFile));
//    std::cout << "Saved img of size " << img.width() << "x" << img.height()
//              << " into " << screenshotSaveFile << std::endl;

//  } else {  // Regular simulation
//    w->setAttribute(Qt::WA_QuitOnClose);
//    w->show();

//    c.nextPlant();
//    c.setAutoQuit(autoQuit);

//    // Setup debug config
//    if (config::Simulation::DEBUG_NO_METABOLISM())
//      c.step();

//    ret = a.exec();

//    s.abort();
//    s.step();
//  }

  // ===========================================================================
  // Final preparations
  w.restoreGeometry(settings.value("geom").toByteArray());
  cs->restoreGeometry(settings.value("cs::geom").toByteArray());
  cs->setVisible(settings.value("cs::visible").toBool());
  w.show();

  v->fitInView(s.bounds(), Qt::KeepAspectRatio);
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
#ifndef NDEBUG
  SAVE(b2DebugDraw, bool)
#endif
#undef SAVE

  return r;
}
