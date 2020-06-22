#include <QApplication>
#include <QMainWindow>
#include <QSettings>
#include <QTimer>
#include <QSplitter>

#include "kgd/external/cxxopts.hpp"

#include "gui/mainview.h"

#include <QDebug>

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

  std::string printMorphoLargerOut;
  std::string loadSaveFile, loadConstraints, loadFields;

//  std::string morphologiesSaveFolder, screenshotSaveFile;

//  int speed = 0;
//  bool autoQuit = false;

//  std::string duration = "=100";
  std::string outputFolder = "tmp/visu_eval/";
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

//    ("show-morphology", "Print the morphology of provided genome",
//     cxxopts::value(cGenomeArg))

//    ("d,duration", "Simulation duration. ",
//     cxxopts::value(duration))
    ("f,data-folder", "Folder under which to store the computational outputs",
     cxxopts::value(outputFolder))
    ("overwrite", "Action to take if the data folder is not empty: either "
                  "[a]bort or [p]urge",
     cxxopts::value(overwrite))
    ("l,load", "Load a previously saved simulation",
     cxxopts::value(loadSaveFile))

    ("show-largest", "Print morphology of largest critter to provided location",
     cxxopts::value(printMorphoLargerOut))

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
//      utils::doThrow<std::invalid_argument>("No value provided for the plant's genome");

//    else if (result.count("plant"))
//      utils::doThrow<std::invalid_argument>("No value provided for the environment's genome");

//    else
//      utils::doThrow<std::invalid_argument>(
//        "No starting state provided. Either provide both an environment and plant genomes"
//        " or load a previous simulation");
//  }

////  if (!morphologiesSaveFolder.empty() && loadSaveFile.empty())
////    utils::doThrow<std::invalid_argument>(
////        "Generating morphologies is only meaningful when loading a simulation");

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  // ===========================================================================
  // == Qt setup

  QApplication a(argc, argv);
  setlocale(LC_NUMERIC,"C");

  QSettings settings ("kgd", "Splinoids");

  QMainWindow w;
  gui::StatsView *stats = new gui::StatsView;
  visu::GraphicSimulation s (w.statusBar(), stats);

  gui::MainView *v = new gui::MainView (s, stats, w.menuBar());

  QSplitter splitter;
  splitter.addWidget(v);
  splitter.addWidget(stats);
  w.setCentralWidget(&splitter);
  w.setWindowTitle("Splinoids main window");

  // ===========================================================================
  // == Simulation setup

  if (loadSaveFile.empty()) {
    std::cerr << "No save file provided... I don't know what to do......\n";

  } else {
    visu::GraphicSimulation::load(loadSaveFile, s, loadConstraints, loadFields);
    if (verbosity != Verbosity::QUIET)
      config::Simulation::printConfig();
  }

  if (!outputFolder.empty()) {
    bool ok = s.setWorkPath(outputFolder, simu::Simulation::Overwrite(overwrite));
    if (!ok)  exit(2);
  }

  if (!printMorphoLargerOut.empty()) {
    const visu::Critter *largest = nullptr;
    float maxMass = 0;
    for (const auto &p: s.critters()) {
      const simu::Critter *sc = p.first;
      if (maxMass < sc->mass()) {
        largest = p.second;
        maxMass = sc->mass();
      }
    }

    std::ostringstream oss;
    oss << printMorphoLargerOut << "_" << maxMass << "kg.png";
    largest->printPhenotype(QString::fromStdString(oss.str()));
    exit(0);
  }

//  if (!duration.empty()) {
//    if (duration.size() < 2)
//      utils::doThrow<std::invalid_argument>(
//        "Invalid duration '", duration, "'. [+|=]<years>");

//    uint dvalue = 0;
//    if (!(std::istringstream (duration.substr(1)) >> dvalue))
//      utils::doThrow<std::invalid_argument>(
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

//  // ===========================================================================
//  // Final preparations
//  w.show();

//  // Load settings
////  qDebug() << "Loaded MainWindow::Geometry:" << settings.value("geometry").toRect();
//  w.setGeometry(settings.value("geometry").toRect());
////  qDebug() << "MainWindow::Geometry:" << w.geometry();


//  v->fitInView(s.bounds(), Qt::KeepAspectRatio);
//  v->selectNext();

//  QTimer::singleShot(100, [&v, startspeed] {
//    if (startspeed) v->start(startspeed);
//    else            v->stop();
//  });

//  auto ret = a.exec();

//  // Save settings
////  qDebug() << "MainWindow::Geometry:" << w.geometry();
//  if (w.geometry().isValid())  settings.setValue("geometry", w.geometry());
////  qDebug() << "Saved MainWindow::Geometry:" << settings.value("geometry").toRect();

//  return ret;
  return 0;
}
