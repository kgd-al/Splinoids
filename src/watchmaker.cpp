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

#include "kgd/external/cxxopts.hpp"

#include "gui/geneticmanipulator.h"
#include "misc/watchmaker_classes.h"

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

int main(int argc, char *argv[]) {  
  // To prevent missing linkages
  std::cerr << config::PTree::rsetSize() << std::endl;
//  std::cerr << phylogeny::SID::INVALID << std::endl;

  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::QUIET;

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

  // ===========================================================================
  // == Qt setup

  QApplication a(argc, argv);
  setlocale(LC_NUMERIC,"C");

  QSettings settings ("kgd", "Splinoids");

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

  rng::FastDice dice (0);

  phylogeny::GIDManager gidManager;
  const auto nextGen = [&] (QWidget *object) {
    SplinoidButton *sender = static_cast<SplinoidButton*>(object);
    const Genome base = sender->critter->object().genotype();
    std::cout << "Generation " << base.gdata.generation << std::endl;

    buttons[mid][mid]->setGenome(s, base);
    manip->setSubject(buttons[mid][mid]->female->critter);

    uint mutations = mutationsCount->value();
    for (uint i=0; i<N; i++) {
      for (uint j=0; j<N; j++) {
        if (i == mid && j == mid) continue;
        Genome g = base;
        g.gdata.updateAfterCloning(gidManager);
        for (uint m=0; m<mutations; m++)  g.mutate(dice);
        buttons[i][j]->setGenome(s, g);
      }
    }
  };

  QSignalMapper mapper;
  QObject::connect(&mapper, &QSignalMapper::mappedWidget, nextGen);

  for (uint i=0; i<N; i++) {
    for (uint j=0; j<N; j++) {

      ulayout->addWidget(buttons[i][j] = new Splinoids,
                        i, j);

      for (SplinoidButton *b: {buttons[i][j]->female, buttons[i][j]->male}) {
        mapper.setMapping(b, b);
        QObject::connect(b, &SplinoidButton::clicked,
                         &mapper, qOverload<>(&QSignalMapper::map));
        b->installEventFilter(&manager);
      }
    }
  }

  buttons[mid][mid]->setGenome(s, genome);
  manip->setSubject(buttons[mid][mid]->female->critter);

  QWidget *bholder = new QWidget;
  auto *rlayout = new QFormLayout;
  bholder->setLayout(rlayout);
  vsplitter->addWidget(bholder);

  rlayout->addRow("Mutation", mutationsCount = new QSpinBox);
  mutationsCount->setMinimum(1);
  mutationsCount->setMaximum(10000);
  mutationsCount->setValue(100);

  w.setCentralWidget(vsplitter);

  w.show();
  buttons[mid][mid]->female->setFocus();

  auto ret = a.exec();

  return ret;
}
