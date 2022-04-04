#include <QApplication>
#include <QMainWindow>
#include <QSettings>
#include <QGridLayout>
#include <QSplitter>
#include <QSpinBox>
#include <QFormLayout>
#include <QSignalMapper>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QProgressDialog>
#include <QMenuBar>

#include "kgd/external/cxxopts.hpp"

#include "../../gui/geneticmanipulator.h"
#include "watchmaker_classes.h"

#include <QDebug>

//#include "config/dependencies.h"
using Genome = genotype::Critter;

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

struct EventManager : public QObject {
  gui::GeneticManipulator &manip;

  EventManager (gui::GeneticManipulator &m) : manip(m) {}

  bool eventFilter (QObject *watched, QEvent *event) {
//    QDebug q = qDebug();
//    q << watched << event;

    switch (event->type()) {
    case QEvent::KeyRelease: {
      QKeyEvent *ke = static_cast<QKeyEvent*>(event);
      if (ke->key() == Qt::Key_C) {
        manip.toggleShow();

        QSettings settings;
        settings.setValue("watching", manip.isVisible());

        return true;
      }
      break;
    }
    case QEvent::MouseButtonRelease: {
      QMouseEvent *me = static_cast<QMouseEvent*>(event);
      SplinoidButton *b = dynamic_cast<SplinoidButton*>(watched);
      if (b && me->button() == Qt::RightButton) {
        manip.setSubject(b->critter);
        manip.show();
        return true;
      }
      break;
    }
    default: break;
    }
    return false;
  }
};

QMenu* createMenu (QMainWindow *w, gui::GeneticManipulator *m) {
#ifndef NDEBUG
//  QMenuBar *mbar = w->menuBar();
//  QMenu *mSplines = mbar->addMenu("Splines");
//  for (uint i=0; i<visu::Critter::SPLINES_COUNT; i++) {
//    QMenu *smSplinesI = mSplines->addMenu(QString::number(i));
//    addAction(smSplinesI, "", "S" + QString::number(i) + "Center", "",
//              Qt::KeypadModifier + (Qt::Key_1+i),
//              [m, i] {
//      config::Visualisation::debugDraw.flip(i);
//      scene()->update();
//    });
//    addAction(smSplinesI, "", "S" + QString::number(i) + "Spline", "",
//              Qt::ShiftModifier + Qt::KeypadModifier + (Qt::Key_1+i),
//              [m, i] {
//      config::Visualisation::debugDraw.flip(i + visu::Critter::SPLINES_COUNT);
//      scene()->update();
//    });
//    addAction(smSplinesI, "", "S" + QString::number(i) + "Edges", "",
//              Qt::ControlModifier + Qt::KeypadModifier + (Qt::Key_1+i),
//              [m, i] {
//      config::Visualisation::debugDraw.flip(i + 2*visu::Critter::SPLINES_COUNT);
//      scene()->update();
//    });
//  }
#endif
  return nullptr;
}

int main(int argc, char *argv[]) {  
  // To prevent missing linkages
  std::cerr << config::PTree::rsetSize() << std::endl;
//  std::cerr << phylogeny::SID::INVALID << std::endl;

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::QUIET;

  bool mutationsAutoLog = false;

  std::string genomeArg = "-1";
  genotype::Critter genome;

  cxxopts::Options options("Splinoids (watchmaker)",
                           "2D simulation of critters in a changing environment");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))

    ("log-mutations", "Whether to automatically log mutations",
     cxxopts::value(mutationsAutoLog)->implicit_value("true"))

    ("g,genome", "Genome to use as starting point (-1 for random seed,"
                 " numeric for specific seed or file)",
     cxxopts::value(genomeArg))
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

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

  long genomeSeed = maybeSeed(genomeArg);
  if (genomeSeed < -1) {
    std::cout << "Reading splinoid genome from input file '"
              << genomeArg << "'" << std::endl;
    genome = Genome::fromFile(genomeArg);

  } else {
    rng::FastDice dice;
    if (genomeSeed >= 0) dice.reset(genomeSeed);
    std::cout << "Generating splinoid genome from rng seed "
              << dice.getSeed() << std::endl;
    genome = Genome::random(dice);
  }

  config::Simulation::setupConfig(configFile, verbosity);

  if (mutationsAutoLog)
    config::EDNAConfigFile_common::autologMutations(true);

  // ===========================================================================
  // == Qt setup

  QApplication a(argc, argv);
  setlocale(LC_NUMERIC,"C");

  QCoreApplication::setOrganizationName("kgd");
  QCoreApplication::setOrganizationDomain("kgd");
  QCoreApplication::setApplicationName("Splinoids-Watchmaker");
  QSettings settings;

  QMainWindow w;
  w.setWindowTitle("Splinoids (watchmaker)");

  gui::GeneticManipulator *manip = new gui::GeneticManipulator (&w);

  EventManager manager (*manip);
  w.installEventFilter(&manager);
  manip->installEventFilter(&manager);

  // ===========================================================================
  // == Simulation setup

  SimulationPlaceholder s;
  s.init();

  QSplitter *vsplitter = new QSplitter (Qt::Vertical);

  QWidget *lholder = new QWidget;
  QGridLayout *ulayout = new QGridLayout;
  lholder->setLayout(ulayout);
  vsplitter->addWidget(lholder);

  static constexpr uint N = 3;
  std::array<std::array<Splinoids*, N>, N> buttons;
  uint mid = (N-1)/2;

  QSpinBox *mutationsCount;
  QLabel *details;
  uint totalMutations;

  rng::FastDice dice;

  phylogeny::GIDManager gidManager;
  genome.gdata.setAsPrimordial(gidManager);

  std::string ofolder = [] {
    std::ostringstream oss;
    oss << "tmp/watchmaker/" << utils::CurrentTime("%Y-%m-%d_%H-%M-%S");
    return oss.str();
  }();
  stdfs::create_directories(ofolder);

  auto imgLog = [&ofolder] (SplinoidButton *b) {
    std::ostringstream ofile;
    ofile << ofolder << "/" << std::setw(5) << std::setfill('0')
          << b->critter->object().genotype().gdata.generation << ".png";
    b->critter->printPhenotype(QString::fromStdString(ofile.str()));
  };

  const auto nextGen = [&] (QWidget *object) {
    SplinoidButton *sender = static_cast<SplinoidButton*>(object);
    const Genome base = sender->critter->object().genotype();

    imgLog(sender);

    QProgressDialog progress ("Mutating...", QString(), 0, 9, &w);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);

    auto start = simu::Simulation::now();

    buttons[mid][mid]->setGenome(s, base);
    buttons[mid][mid]->setFocus();

    uint mutations = mutationsCount->value();
    for (uint i=0; i<N; i++) {
      for (uint j=0; j<N; j++) {
        progress.setValue(i*N+j);

        if (i == mid && j == mid) continue;
        Genome g = base;
        g.gdata.updateAfterCloning(gidManager);
        if (config::EDNAConfigFile_common::autologMutations()) {
          std::cerr << "Processing " << i << "x" << j << ": ";
          if (mutations > 1)  std::cerr << "\n";
        }
        for (uint m=0; m<mutations; m++)  g.mutate(dice);
        buttons[i][j]->setGenome(s, g);
      }
    }

    progress.setValue(N*N);
    std::cout << "Generation " << base.gdata.generation
              << " computed in " << simu::Simulation::durationFrom(start)
              << " ms" << std::endl;

    if (sender->parentWidget() != buttons[mid][mid])
      totalMutations += mutations;

    QString str;
    QTextStream qts (&str);
    qts << "Generation " << base.gdata.generation << ". "
        << totalMutations << " total mutations";
    details->setText(str);
  };

  QSignalMapper mapper;
  QObject::connect(&mapper, &QSignalMapper::mappedWidget, nextGen);

  for (uint i=0; i<N; i++) {
    for (uint j=0; j<N; j++) {

      ulayout->addWidget(buttons[i][j] = new Splinoids,
                         i, j);

      for (SplinoidButton *b: {
#ifdef USE_DIMORPHISM
        buttons[i][j]->female, buttons[i][j]->male
#else
        buttons[i][j]->button
#endif
        }) {
        mapper.setMapping(b, b);
        QObject::connect(b, &SplinoidButton::clicked,
                         &mapper, qOverload<>(&QSignalMapper::map));
        b->installEventFilter(&manager);

        QObject::connect(b, &SplinoidButton::focused,
                         [manip, b] { manip->setSubject(b->critter); });
      }
    }
  }

  buttons[mid][mid]->setGenome(s, genome);
#ifdef USE_DIMORPHISM
  imgLog(buttons[mid][mid]->female);
#else
  imgLog(buttons[mid][mid]->button);
#endif

  QWidget *bholder = new QWidget;
  auto *rlayout = new QFormLayout;
  bholder->setLayout(rlayout);
  vsplitter->addWidget(bholder);

  vsplitter->setStretchFactor(0,1);
  vsplitter->setStretchFactor(1,0);

  rlayout->addRow("Mutation", mutationsCount = new QSpinBox);
  mutationsCount->setMinimum(1);
  mutationsCount->setMaximum(10000);
  mutationsCount->setValue(100);
  mutationsCount->setFocusPolicy(Qt::NoFocus);

  rlayout->addRow(details = new QLabel ("Initial generation"));

  w.setCentralWidget(vsplitter);

  w.restoreGeometry(settings.value("window").toByteArray());
  manip->restoreGeometry(settings.value("watcher").toByteArray());
  manip->setVisible(settings.value("watching").toBool());
  mutationsCount->setValue(settings.value("mutations").toInt());

  QObject::connect(&a, &QApplication::aboutToQuit,
                   [&] {
    settings.setValue("window", w.saveGeometry());
    settings.setValue("watcher", manip->saveGeometry());
//    settings.setValue("watching", manip->isVisible());
    settings.setValue("mutations", mutationsCount->value());
  });

  w.show();
  buttons[mid][mid]->setFocus();

  auto ret = a.exec();

  return ret;
}
