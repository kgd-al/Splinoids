#include <QApplication>
#include <QMainWindow>
#include <QSettings>
#include <QTimer>
#include <QSplitter>

#include "kgd/external/cxxopts.hpp"

#include "gui/mainview.h"
#include "visu/config.h"

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

struct RGB {
  QRgb c;
  friend std::ostream& operator<< (std::ostream& os, const RGB &wrapper) {
    const QRgb &c = wrapper.c;
    return os << "(" << qAlpha(c) << " " << qRed(c) << " " << qGreen(c)
              << " " << qBlue(c) << ")";
  }
};

void doBlendedPrint (const visu::GraphicSimulation &s,
                     const stdfs::path &folder) {
  struct Key {
    simu::Critter::Sex sex;
    decltype(simu::Critter::userIndex) ui;

    Key (const simu::Critter &c) : sex(c.sex()), ui(c.userIndex) {}
    Key (const visu::Critter &v) : Key(v.object()) {}
  };
  struct CMP {
    bool operator() (const Key &lhs, const Key &rhs) const {
      if (lhs.ui != rhs.ui) return lhs.ui < rhs.ui;
      return lhs.sex < rhs.sex;
    }
  };
  struct Data {
    QSizeF maxSize = QSize(0,0);
    std::vector<visu::Critter*> vcritters;
  };
  std::map<Key,Data,CMP> data;

  for (auto &entry: s.critters()) {
    const simu::Critter *c = entry.first;
    visu::Critter *v = entry.second;
    Key k (*v);
    Data &d = data[k];

    if (c->isYouth()) continue;

    d.vcritters.push_back(v);
    d.maxSize = d.maxSize.expandedTo(v->critterBoundingRect().size());
  }

  struct FColor {
    float a, r, g, b;

    FColor (void) : a(0), r(0), g(0), b(0) {}

    void add (QRgb c, float w) {
      a += w * qAlpha(c) / 255.f;
      r += w * qRed(c) / 255.f;
      g += w * qGreen(c) / 255.f;
      b += w * qBlue(c) / 255.f;
    }

    QRgb toQRgb (void) const {
      return qRgba(
        std::round(r*255),
        std::round(g*255),
        std::round(b*255),
        std::round(a*255));
    }
  };
  for (auto &entry: data) {
    const Key &k = entry.first;
    Data &d = entry.second;

    QSize size = d.maxSize.toSize() * config::Visualisation::viewZoom();
    std::vector<FColor> fcolors;
    fcolors.resize(size.width()*size.height());

    const auto IX = [size] (uint row, uint col) {
      return row*size.width()+col;
    };

    QImage img = QImage(size, QImage::Format_ARGB32);

    float alpha = 1. / double(d.vcritters.size());

    std::cerr << "I" << k.ui << "S" << int(k.sex) << ": "
              << d.vcritters.size() << "\n";

    std::cerr << "Alpha is: " << alpha << "\n";

    for (const auto &v: d.vcritters) {
      QImage vi = v->renderPhenotype().toImage()
          .convertToFormat(QImage::Format_ARGB32);

//      qDebug() << "vi type: " << vi.format();
      int xoff = .5f * (img.width() - vi.width());
      int yoff = .5f * (img.height() - vi.height());

//      std::cout << "Copying from image of size " << vi.width() << " x "
//                << vi.height() << "\n";
      for (int y=0; y<vi.height(); y++) {
//        QRgb *v_p = (QRgb*)img.scanLine(y+yoff);
        QRgb *vi_p = (QRgb*)vi.scanLine(y);

        for (int x=0; x<vi.width(); x++) {
          if (qAlpha((vi_p[x])) > 0) {
            const QRgb &in = vi_p[x];
            FColor &out = fcolors[IX(y+yoff, x+xoff)];
            out.add(in, alpha);

//            std::cerr << "v_p[" << y+yoff << "][" << x+xoff << "] = "
//              << RGB{out} << " + " << RGB{in} << " * " << alpha
//              << ") = ";
//            out = qRgba(
//                qRed(out) + qRed(in) * alpha,
//                qGreen(out) + qGreen(in) * alpha,
//                qBlue(out) + qBlue(in) * alpha,
//                qAlpha(out) + alpha * 255);
//            out = qRgba(0,0,255,255);
//            std::cerr << RGB{out} << "\n";
          }
        }
      }

      for (int y=0; y<img.height(); y++) {
        QRgb *v_p = (QRgb*)img.scanLine(y);

        for (int x=0; x<img.width(); x++)
          v_p[x] = fcolors[IX(y, x)].toQRgb();
      }
    }

    std::ostringstream oss;
    oss << "blended_I" << k.ui << "S" << int(k.sex) << ".png";
    stdfs::path outfile = folder / oss.str();
    if (img.save(QString::fromStdString(outfile), "png", 100))
      std::cout << "Saved";
    else
      std::cout << "Failed to save";
    std::cout << " " << outfile << " of size " << img.width() << "x"
                << img.height() << std::endl;
  }
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

  std::string printAllOut, printMorphoLargerOut, printBlendedOut;
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

    ("show-all", "Print morphology of all critters to provided location",
     cxxopts::value(printAllOut))

    ("show-largest", "Print morphology of largest critter to provided location",
     cxxopts::value(printMorphoLargerOut))

    ("show-blend", "Print blended morphologies of critters (grouped by sex and "
                   "user index) to provided location",
     cxxopts::value(printBlendedOut))

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

  if (!printAllOut.empty()) {
    for (const auto &entry: s.critters()) {
      std::ostringstream oss;
      oss << "C" << std::hex << entry.first->id() << ".png";
      stdfs::path path = stdfs::path(printAllOut) / oss.str();
      entry.second->printPhenotype(QString::fromStdString(path));
    }
  }

  if (!printBlendedOut.empty())
    doBlendedPrint(s, printBlendedOut);

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
