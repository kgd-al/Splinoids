#include <csignal>

#include "../simu/simulation.h"

#include "kgd/external/cxxopts.hpp"

#ifndef CLUSTER_BUILD
#include <QApplication>
#include "../visu/graphicsimulation.h"
#include "../visu/config.h"
#endif

using CGenome = genotype::Critter;
using EGenome = genotype::Environment;

std::atomic<bool> aborted = false;
void sigint_manager (int) {
  std::cerr << "Gracefully exiting simulation "
               "(please wait for end of current step)" << std::endl;
  aborted = true;
}

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

using json = nlohmann::json;
static const json env_json = R"({"mvp":1,"size":4,"taurus":1})"_json;
static const int spacing = 1;

static const json agg_json =
  R"({"asexual":1,"bdepth":2,"cdata":{"S":1,"mu":1.5255935192108154,"si":2.0,"so":2.0},"colors":[[0.2757517397403717,0.4613220989704132,0.2592845559120178],[0.6694125533103943,0.257860004901886,0.7085900902748108],[0.5034935474395752,0.48779943585395813,0.28763362765312195],[0.48798668384552,0.5404545664787292,0.3623172640800476],[0.6956374645233154,0.7438890933990479,0.4656842052936554],[0.36192917823791504,0.6419539451599121,0.3971322178840637],[0.5745621919631958,0.2712758481502533,0.3819182515144348],[0.718996524810791,0.676059365272522,0.5456474423408508],[0.40456676483154297,0.3773477077484131,0.33870458602905273],[0.5444707870483398,0.6040284633636475,0.28086891770362854]],"dimorphism":[0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0],"gen":{"f":{"g":4294967295,"s":4294967295},"m":{"g":1,"s":4294967295},"s":{"g":0,"s":4294967295}},"hNeat":{"data":[7,2,[[1,1,0.0,1,0.0,0.0,0.0,0.0],[2,1,0.0,1,0.0,0.0,0.0,0.0],[3,1,0.0,1,0.0,0.0,0.0,0.0],[4,1,0.0,1,0.0,0.0,0.0,0.0],[5,1,0.0,1,0.0,0.0,0.0,0.0],[6,1,0.0,1,0.0,0.0,0.0,0.0],[7,2,0.0,1,0.0,0.0,0.0,0.0],[8,4,1.0,2,3.025,0.0,0.0,0.0],[9,4,1.0,2,3.025,0.0,0.0,0.0]],[[1,8,1,false,-0.2224141622054316],[2,8,2,false,-0.2893479423074784],[3,8,3,false,-0.2623941856224721],[4,8,4,false,0.07122884209669722],[5,8,5,false,-0.07434555406294124],[6,8,6,false,0.331783829828021],[7,8,7,false,0.15527017243522534],[1,9,8,false,0.27539565995951865],[2,9,9,false,-0.10105442860410951],[3,9,10,false,0.30601693733333146],[4,9,11,false,0.13018283353913396],[5,9,12,false,-0.2861922949246051],[6,9,13,false,0.10476210702341515],[7,9,14,false,0.012611255834330759]]],"hnl":1,"hnv":1},"mature":0.33000001311302185,"maxCS":1.0,"minCS":1.0,"old":0.6600000262260437,"splines":[{"data":[0.9738937616348267,-0.816814124584198,0.3700000047683716,0.0,0.48000001907348633,1.0,0.0,0.4449999928474426,0.0,0.5]},{"data":[1.5707963705062866,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0]},{"data":[1.5707963705062866,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0]},{"data":[1.5707963705062866,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0]}],"vision":{"angleBody":0.5235987901687622,"angleRelative":0.0,"precision":1,"width":1.0471975803375244}})"_json;

static const json def_json =
  R"({"asexual":1,"bdepth":2,"cdata":{"S":1,"mu":3.716581106185913,"si":2.0,"so":2.0},"colors":[[0.7463167905807495,0.37734198570251465,0.6041678190231323],[0.723727285861969,0.5157703161239624,0.6358364820480347],[0.41869887709617615,0.6319261789321899,0.3835113048553467],[0.35868561267852783,0.6567767858505249,0.4212232232093811],[0.5429543256759644,0.6055812239646912,0.3682592511177063],[0.3386257290840149,0.28748273849487305,0.39409199357032776],[0.28446635603904724,0.4578661620616913,0.38329559564590454],[0.37569767236709595,0.39282092452049255,0.633774995803833],[0.6629076600074768,0.6410514116287231,0.5506556034088135],[0.3471841514110565,0.3441762924194336,0.46281373500823975]],"dimorphism":[0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0],"gen":{"f":{"g":4294967295,"s":4294967295},"m":{"g":1,"s":4294967295},"s":{"g":5,"s":4294967295}},"hNeat":{"data":[7,2,[[1,1,0.0,1,0.0,0.0,0.0,0.0],[2,1,0.0,1,0.0,0.0,0.0,0.0],[3,1,0.0,1,0.0,0.0,0.0,0.0],[4,1,0.0,1,0.0,0.0,0.0,0.0],[5,1,0.0,1,0.0,0.0,0.0,0.0],[6,1,0.0,1,0.0,0.0,0.0,0.0],[7,2,0.0,1,0.0,0.0,0.0,0.0],[8,4,1.0,2,3.025,0.0,0.0,0.0],[9,4,1.0,2,3.025,0.0,0.0,0.0]],[[1,8,1,false,0.14466649659060304],[2,8,2,false,0.16571769576844209],[3,8,3,false,0.09983673131406512],[4,8,4,false,0.22465679478980638],[5,8,5,false,0.3806830012141498],[6,8,6,false,0.23475786514491026],[7,8,7,false,-0.3168707449042088],[1,9,8,false,-0.012957573157536373],[2,9,9,false,0.013044770053520616],[3,9,10,false,-0.21084683843428137],[4,9,11,false,0.1152910997911547],[5,9,12,false,0.24432266712397832],[6,9,13,false,0.28268882022029795],[7,9,14,false,-0.13396260903750606]]],"hnl":1,"hnv":1},"mature":0.33000001311302185,"maxCS":1.0,"minCS":1.0,"old":0.6600000262260437,"splines":[{"data":[0.816814124584198,1.1938053369522095,0.5,0.0,-0.12000000476837158,1.0,0.7000000476837158,0.25,0.5,0.0]},{"data":[1.5707963705062866,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0]},{"data":[1.5707963705062866,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0]},{"data":[1.5707963705062866,0.0,0.0,0.0,0.0,1.0,0.0,0.0,0.0,0.0]}],"vision":{"angleBody":0.5235987901687622,"angleRelative":0.0,"precision":1,"width":1.0471975803375244}})"_json;

class TestSimulationHolder {
public:
  simu::Simulation *s = nullptr;

private:
  using Critter = simu::Critter;
  Critter *c0, *c1;
  bool combatStarted, combatFinished, immobile;

  stdfs::path logPath;
  std::ofstream log;

#ifndef CLUSTER_BUILD
  auto guiSimulation (void) {
    return dynamic_cast<visu::GraphicSimulation*>(s);
  }

  static auto screenshotPath (simu::Simulation *s) {
    std::ostringstream oss;
    oss << "step" << std::setfill('0') << std::setw(5)
        << s->currTime().timestamp() << ".png";
    return s->localFilePath(oss.str());
  }

  static void screenshot (visu::GraphicSimulation *gs) {
//    std::cout << "Creating screenshot " << path << "\n";
    gs->render(QString::fromStdString(screenshotPath(gs)));
  }
#endif

public:
  bool finished (void) const {
    return (combatStarted && combatFinished) || immobile
        || s->aborted() || s->critters().size() != 2;
  }

  template <typename Simulation>
  void init (float f, float a, bool flipped,
             const stdfs::path &lpath, simu::Simulation::Overwrite o) {
    typename Simulation::InitData idata {};
    idata.ienergy = 0;
//    idata.cRatio = 1. / (1+config::Simulation::plantEnergyDensity());
    idata.nCritters = 0;
//    idata.cRange = .25;
//    idata.pRange = 1;
    idata.seed = -1;

    auto eGenome = EGenome(env_json);

    std::vector<CGenome> cgenomes { CGenome(agg_json), CGenome(def_json) };

    combatStarted = combatFinished = immobile = false;

    s = new Simulation ();
    s->setWorkPath(lpath, o);
    s->init(eGenome, cgenomes, idata);

    auto e = Critter::maximalEnergyStorage(Critter::MAX_SIZE);
    c0 = s->addCritter(cgenomes[flipped], -spacing, 0, 0, e, .5);
    c1 = s->addCritter(cgenomes[1-flipped], +spacing, 0, a, e, .5);

    c0->brainDead = true;
    c1->brainDead = true;

    c0->body().ApplyLinearImpulseToCenter({f,0}, true);

    logPath = s->localFilePath("test.log");
    log.open(logPath);
    doLog(false, true);
  }

  void step (void) {
    s->step();

    bool fighting = !s->environment().fightingEvents().empty();
    if (!combatStarted) combatStarted = fighting;
    else  combatFinished = !fighting;

    immobile = (c0->body().GetLinearVelocity().Length() < 1e-3)
            && (c1->body().GetLinearVelocity().Length() < 1e-3);

    if (s->critters().size() == 2)  doLog(fighting);

#ifndef CLUSTER_BUILD
    if (auto gs = guiSimulation())  screenshot(gs);
#endif
  }

  void doLog (bool fighting, bool header = false) {
    if (header) {
      log << "time fight";
      for (uint i=0; i<2; i++)
        log << " c" << i << "x c" << i << "y c" << i << "lv"
            << " c" << i << "bh c" << i << "ls c" << i << "rs";
      log << "\n";
    }

    log << s->currTime().timestamp() << " " << fighting;
    for (const Critter *c: {c0, c1})
      log << " " << c->x() << " " << c->y()
          << " " << c->body().GetLinearVelocity().Length()
          << " " << c->bodyHealthness()
          << " " << c->splineHealthness(0, Critter::Side::LEFT)
          << " " << c->splineHealthness(0, Critter::Side::RIGHT);
    log << "\n";
  }

  void atEnd (void) {
    s->atEnd();

    std::string log = logPath;
    std::string graphs = logPath.parent_path() / "test.png";

    std::ostringstream oss;
    oss << "gnuplot -e \""
            "set terminal pngcairo size 1680,1050;\n"
            "set output '" << graphs << "';\n"
            "set key above;\n"
            "set autoscale fix;\n"
            "set grid;\n"
            "set multiplot layout 2,2;\n"
            "set xrange [-2:2];\n"
            "set yrange [-2:2];\n"
            "plot '" << log << "' u 3:4 w l title columnhead,"
                              "'' u 9:10 w l title columnhead;\n"
            "set xrange [*:*];\n"
            "set yrange [*:*];\n"
            "set y2range [-.05:1.05];\n"
            "plot '" << log << "' u 2 w l axes x1y2 title 'fight?',"
                              "'' u 5 w l title columnhead,"
                              "'' u 11 w l title columnhead;\n"
            "set yrange [-.05:1.05];\n"
            "plot for [i=6:8] '" << log << "' u i w l title columnhead;\n"
            "plot for [i=12:14] '" << log << "' u i w l title columnhead;\n"
            "unset multiplot;\n\"";

    std::string cmd = oss.str();

    this->log.close();
    auto res = system(cmd.c_str());
    if (res == 0)
      std::cout << "Generated " << graphs << std::endl;
    else
      std::cerr << "Failed to execute:\n" << cmd << std::endl;

#ifndef CLUSTER_BUILD
    if (auto gs = guiSimulation()) {
      std::string img = screenshotPath(gs);
      std::string summary = s->localFilePath("summary.png");
      oss.str("");
      oss << "montage -geometry '1680x1050<' " << graphs << " " << img
          << " " << summary;
      cmd = oss.str();

      auto res = system(cmd.c_str());
      if (res == 0)
        std::cout << "Generated " << summary << std::endl;
      else
        std::cerr << "Failed to execute:\n" << cmd << std::endl;
    }
#endif

    delete s;
    s = nullptr;
  }
};

int main(int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

  using Verbosity = config::Verbosity;

  std::string configFile = "auto";  // Default to auto-config
  Verbosity verbosity = Verbosity::SHOW;

//  std::string duration = "=100";
  std::string outputFolder = "tmp_test";
  char overwrite = simu::Simulation::UNSPECIFIED;

#ifndef CLUSTER_BUILD
  bool withgui=false;
  // To prevent missing linkages
  std::cerr << config::PTree::rsetSize() << std::endl;

  QApplication *qapp;
#endif

  cxxopts::Options options("Splinoids (tester)",
                           "Automated test battery");
  options.add_options()
    ("h,help", "Display help")
    ("a,auto-config", "Load configuration data from default location")
    ("c,config", "File containing configuration data",
     cxxopts::value(configFile))
    ("v,verbosity", "Verbosity level. " + config::verbosityValues(),
     cxxopts::value(verbosity))

//    ("d,duration", "Simulation duration. ",
//     cxxopts::value(duration))
    ("f,data-folder", "Folder under which to store the computational outputs",
     cxxopts::value(outputFolder))
    ("overwrite", "Action to take if the data folder is not empty: either "
                  "[a]bort or [p]urge",
     cxxopts::value(overwrite))

#ifndef CLUSTER_BUILD
    ("gui", "Also perform rendering of simulation steps to provide visual"
            " feedback",
     cxxopts::value(withgui))
#endif
    ;

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
              << std::endl;
    return 0;
  }

  if (result.count("auto-config") && result["auto-config"].as<bool>())
    configFile = "auto";

//    config::Simulation::setupConfig(configFile, verbosity);
  if (verbosity != Verbosity::QUIET) config::Simulation::printConfig(std::cout);
  if (configFile.empty()) config::Simulation::printConfig("");

  // ===========================================================================
  // == SIGINT management

  struct sigaction act = {};
  act.sa_handler = &sigint_manager;
  if (0 != sigaction(SIGINT, &act, nullptr))
    utils::doThrow<std::logic_error>("Failed to trap SIGINT");
  if (0 != sigaction(SIGTERM, &act, nullptr))
    utils::doThrow<std::logic_error>("Failed to trap SIGTERM");

  // ===========================================================================
  // == Core setup

  auto start = simu::Simulation::now();

  std::vector<float> speeds { 5/2.f, 15/4.f, 5.f };
  std::vector<float> angles { 0, M_PI/2., M_PI };

  simu::Simulation::printStaticStats();

#ifndef CLUSTER_BUILD
  if (withgui) {
    qapp = new QApplication(argc, argv);
  }
#endif
  TestSimulationHolder tsh;

  auto eoverwrite = simu::Simulation::Overwrite(overwrite);

  uint total = speeds.size() * angles.size() * 2, processed = 0;

  for (float f: speeds) {
    for (float a: angles) {
      for (uint i=0; i<2; i++) {

        std::string type = i==0?"regular":"flipped";
        std::ostringstream oss;
        oss << "f" << int(100*f)/100.f << "_a" << a/M_PI << "_" << type;
        stdfs::path lpath = stdfs::path(outputFolder) / oss.str();

        std::cout << "\n["
                  << std::setw(3) << int(100*(processed++)/total)
                  << "%] Testing with impulse of " << f << ", angle of " << a
                  << " (" << type << ")\n";

#ifndef CLUSTER_BUILD
        if (withgui)
          tsh.init<visu::GraphicSimulation>(f, a, i, lpath, eoverwrite);
        else
#endif
          tsh.init<simu::Simulation>(f, a, i, lpath, eoverwrite);

        while (!tsh.finished()) {
          if (aborted)  tsh.s->abort();
          tsh.step();
        }
        tsh.atEnd();

        std::cout.flush();
        std::cerr.flush();
      }
    }
  }

  std::cout << "\n[100%] Tests completed in "
            << simu::Simulation::durationFrom(start)
            << "s" << std::endl;

#ifndef CLUSTER_BUILD
  if (qapp) delete qapp;
#endif

//  s.destroy();
  return 0;
}
