#include "indevaluator.h"

namespace simu {

std::atomic<bool> Evaluator::aborted = false;

Evaluator::Evaluator (const std::string &scenarioType,
                      const std::string &scenarioArg) {
  params = Params::fromArgv(scenarioType, scenarioArg);
}

std::string Evaluator::prettyEvalTypes(void) {
  std::ostringstream oss;
  oss << "[ d c22 ]";
  return oss.str();
}

void Evaluator::applyNeuralFlags(phenotype::ANN &ann,
                                 const std::string &tagsfile) {
  auto &n = ann.neurons();

  std::ifstream ifs (tagsfile);
  if (!ifs)
    utils::Thrower("Failed to open neural tags file '", tagsfile, "'");

  for (std::string line; std::getline(ifs, line); ) {
    if (line.empty() || line[0] == '/') continue;
    std::istringstream iss (line);
    phenotype::Point pos;
    iss >> pos;

    auto it = n.find(pos);
    if (it == n.end())
      utils::Thrower("No neuron at position {", pos.x(), ", ", pos.y(), "}");

    iss >> (*it)->flags;
  }

  // Also aggregate similar inputs/outputs
  static constexpr auto EPSILON = 1e-3;
  for (auto &p: ann.neurons()) {
    using P = phenotype::Point;
    phenotype::ANN::Neuron &n = *p;
    P pos = n.pos;
    auto &f = n.flags;

    if (n.type == phenotype::ANN::Neuron::I) {
#ifdef WITH_SENSORS_HEALTH
      if (pos == P{0.f, -1.f, -.5f} || pos == P{0.f, -1.f, -1.f})
        f = 1<<18;  // health
      else
#endif
      if (pos.z() <= -1/3.f + EPSILON) {
        f = 1<<19;  // audition
        if (pos.x() > 0)  f |= 1<<17;  // right
      } else if (pos.z() >= 1/3.f - EPSILON) {
        f = 1<<20;  // vision
        if (pos.x() > 0)  f |= 1<<17;  // right
#ifdef WITH_SENSORS_TOUCH
      } else {
        f = 1<<21;  // touch
        if (pos.x() > 0)  f |= 1<<17;  // right
#endif
      }
      if (f) f |= 1<<16; // input

    } else if (n.type == phenotype::ANN::Neuron::O) {
      if (pos.z() >= .5)
        f = 1 << 26;  // Voice
      else if (pos.z() == -.5f) {
        f = 1<<27;    // arms
        if (pos.x() > 0) f |= 1<<25;  // right
      }
      if (f) f |= 1<<24; // output
    }
  }

  if (config::Simulation::verbosity() > 0) {
    std::cout << "Neural flags:\n";
    for (const auto &p: n) {
      std::ostringstream oss;
      oss.precision(3);
      oss << p->pos;
      std::cout << std::setw(30) << oss.str() << ":\t"
                << std::bitset<8*sizeof(p->flags)>(p->flags)
                << " (" << p->flags << ")"
                << "\n";
    }
  }
}

auto duration (const simu::Simulation &s) {
  static const auto &DT = config::Simulation::ticksPerSecond();
  return float(s.currTime().timestamp()) / DT;
}

template <typename T, typename ...ARGS>
auto avg (const std::set<Critter*> &pop,
          T (Critter::*getter) (ARGS...) const,
          ARGS ...args) {
  T v = 0;
  if (pop.size() > 0) {
    for (Critter *c: pop)  v += (c->*getter)(std::forward<ARGS>(args)...);
    v /= T(pop.size());
  }
  return v;
}

// === Logging
/// TODO Log (what to log?)

struct Evaluator::LogData {
  std::ofstream traj, adata, // fight / accoustics
                ndata; // hidden neurons / modules

  std::vector<std::ofstream> idata, odata, // neural i/o
                             mdata; // modules
};

Evaluator::LogData* Evaluator::logging_getData(void) {
  return new Evaluator::LogData;
}

void Evaluator::logging_freeData(LogData **d) {
  delete *d;
  *d = nullptr;
}

void Evaluator::logging_init(LogData *d, const stdfs::path &f,
                             Scenario &s) {

  if (stdfs::create_directories(f))
    std::cout << "Created " << f << "\n";
  if (!stdfs::exists(f))
    utils::Thrower("Failed to create data folder ", f);

  static const auto NEURAL_FLAGS =
    "AE2;AE1;AE0;AR2;AR1;AR0;VE2;VE1;VE0;VR";

  const auto filename = [] (const std::string &name,
                            const std::string &ext,
                            uint i) {
    std::string id = utils::mergeToString("_", Scenario::critterRole(i));
    return utils::mergeToString(name, id, ".", ext);
  };

  const auto &critters = s.critters();

  d->traj.open(f / "trajectory.dat");
  d->traj << "t X Y A\n";

  d->adata.open(f / "acoustics.dat");
  auto &alog = d->adata;
  for (uint i=0; i<critters.size(); i++) {
    std::string prefix = std::string(1, std::toupper(Scenario::critterRole(i)));
    for (auto s: {'L', 'R'}) {
      alog << prefix << "I" << s << "N ";
      for (uint i=0; i<Critter::VOCAL_CHANNELS; i++)
        alog << prefix << "I" << s << i << " ";
    }
    alog << prefix << "ON ";
    for (uint i=0; i<Critter::VOCAL_CHANNELS; i++)
      alog << prefix << "O" << i << " ";
  }
  alog << "\n";

  d->idata.resize(critters.size());
  d->odata.resize(critters.size());
  for (uint i=0; i<critters.size(); i++) {
    auto &ilog = d->idata[i];
    ilog.open(f / filename("inputs", "dat", i));
    for (auto s: critters[0]->neuralInputsHeader()) ilog << s << " ";
    ilog << "\n";

    auto &olog = d->odata[i];
    olog.open(f / filename("outputs", "dat", i));
    for (auto s: critters[0]->neuralOutputsHeader()) olog << s << " ";
    olog << "\n";
  }

  if (s.neuralEvaluation() && annTagsFile.empty()) {
    auto &nlog = d->ndata;
    nlog.open(f / "neurons.dat");
    nlog << NEURAL_FLAGS;
    for (const auto &p: critters[0]->brain().neurons())
      if (p->isHidden())
        nlog << " (" << p->pos << ")";
    nlog << "\n";
  }

  if (!annTagsFile.empty()) {
    auto &mlogs = d->mdata;
    mlogs.resize(critters.size());
    for (uint i=0; i<mlogs.size(); i++) {
      auto &mlog = mlogs[i];
      mlog.open(f / filename("modules", "dat", i));
      mlog << NEURAL_FLAGS;
      for (const auto &p: manns[i]->modules()) {
        if (p.second->type() == phenotype::ANN::Neuron::H) {
          auto f = p.second->flags;
          mlog << " " << f << "M " << f << "S";
        }
      }
      mlog << "\n";
    }
  }

  {
    std::ofstream blog (f / "brain.dat");
    const phenotype::ANN &b = critters[0]->brain();
    blog << "Type X Y Z Depth Flags\n";
    for (const phenotype::ANN::Neuron::ptr &p: b.neurons()) {
      blog << p->type
           << " " << p->pos.x() << " " << p->pos.y() << " " << p->pos.z()
           << " " << p->depth << " " << p->flags << "\n";
    }

#ifdef WITH_GVC
//    b.render_gvc_graph(f / "brain.dot");
#endif
  }
}

void Evaluator::logging_step(LogData *d, Scenario &s) {
  const auto &critters = s.critters();

  if (d->adata.is_open()) {
    auto &alog = d->adata;
    for (const simu::Critter *c: critters) {
      for (auto v: c->ears()) alog << v << " ";
      for (auto v: c->producedSound()) alog << v << " ";
    }
    alog << "\n";
  }

  for (uint i=0; i<critters.size(); i++) {
    auto &ilog = d->idata[i];
    if (ilog.is_open()) {
      for (const auto &v: critters[i]->neuralInputs()) ilog << v << " ";
      ilog << "\n";
    }

    auto &olog = d->odata[i];
    if (olog.is_open()) {
      for (const auto &v: critters[i]->neuralOutputs()) olog << v << " ";
      olog << "\n";
    }
  }

  if (s.neuralEvaluation()) {
    auto &os = d->ndata;
    os << s.currentFlags();
    for (const auto &p: critters[0]->brain().neurons())
      if (p->isHidden())
        os << " " << p->value;
    os << "\n";
  }

  for (uint i=0; i<d->mdata.size(); i++) {
    auto &os = d->mdata[i];
    os << s.currentFlags();
    for (const auto &p: manns[i]->modules()) {
      if (p.second->type() == phenotype::ANN::Neuron::H) {
        const auto &v = p.second->value();
        os << " " << v.mean << " " << v.stddev;
      }
    }
    os << "\n";
  }
}

// ===
static constexpr Color RED    {{1.f, 0.f, 0.f}};
static constexpr Color GREEN  {{0.f, 1.f, 0.f}};
static constexpr Color BLUE   {{0.f, 0.f, 1.f}};
static std::vector<Color> colors (uint n) {
  if (n == 2)
    return {RED, BLUE};
  else if (n == 3)
    return {RED, GREEN, BLUE};
  else
    return {};
}

bool rgb_cmp (const Color &lhs, const Color &rhs) {
  return !std::lexicographical_compare(lhs.begin(), lhs.end(),
                                       rhs.begin(), rhs.end());
}

void parseDirectionSpecs (const std::string &s, Evaluator::Params &p) {
  static const auto spec = [] (uint t) {
    Scenario::Spec s {};
    s.target = t;
    s.name = utils::mergeToString("d_", "lfr"[t]);
    return s;
  };

  if (s.size() == 1) {
    p.specs = {spec(0), spec(1), spec(2)};

  } else if (s.size() == 3) {
    Scenario::Spec spec {};
    switch (s[2]) {
    case 'l': spec.target = 0; break;
    case 'f': spec.target = 1; break;
    case 'r': spec.target = 2; break;
    default:
      utils::Thrower("Unknown direction scenario subtype ", s[2]);
    }
    spec.name = s;
    p.specs = {spec};

  } else
    utils::Thrower("Malformed direction scenario specification '", s, "'");
}

using Colors = std::vector<Color>;
void parseColorSpace (const std::string &s, uint &cl, uint &cr,
                      Colors &l_colors, Colors &r_colors) {
  if (s.size() < 3)
    utils::Thrower("Missing argument(s) for color scenario: '", s,
                   "' != c[23][23]");

  cl = s[1] - '0';
  l_colors = colors(cl);
  if (l_colors.empty())
    utils::Thrower("Invalid value ", cl, " for left-hand side colors");

  cr = s[2] - '0';
  r_colors = colors(cr);
  if (r_colors.empty())
    utils::Thrower("Invalid value ", cr, " for right-hand side colors");
}

Colors nthPermutation (Colors colors, uint n) {
  for (uint i=0; i<n; i++)
    std::next_permutation(colors.begin(), colors.end(), rgb_cmp);
  return colors;
}

void parseColorSpecs (const std::string &s, Evaluator::Params &p) {
  uint cl, cr;
  Colors l_colors, r_colors;
  parseColorSpace(s, cl, cr, l_colors, r_colors);

  if (s.size() == 3) {
    uint i_r = 0;
    for (const Color &r_c: r_colors) {
      l_colors = colors(cl);

      uint i_l = 0;
      do {
        Scenario::Spec spec;
        spec.colors = l_colors;
        spec.colors.push_back(r_c);
        spec.name = utils::mergeToString("c", cl, cr, "_", i_l, i_r);
        p.specs.push_back(spec);
        i_l++;

      } while (std::next_permutation(l_colors.begin(), l_colors.end(), rgb_cmp));

      i_r++;
    }

  } else if (s.size() == 6) {
    uint cl_id = s[4] - '0', cr_id = s[5] - '0';
    if (cl_id >= cl)
      utils::Thrower("Unknown color scenario (left) subtype '", s[4], "'");
    if (cr_id >= cr)
      utils::Thrower("Unknown color scenario (right) subtype '", s[5], "'");

    Scenario::Spec spec;
    spec.colors = nthPermutation(colors(cl), cl_id);
    spec.colors.push_back(colors(cr)[cr_id]);
    spec.name = utils::mergeToString("c", cl, cr, "_", cl_id, cr_id);
    p.specs = {spec};

  } else
    utils::Thrower("Malformed color scenario specification '", s, "'");
}

std::vector<float> parseAudioSample (const stdfs::path &file,
                                     Scenario::Params::Flag flag) {
  std::ifstream ifs (file);
  if (!ifs)
    utils::Thrower("Failed to open '", file, "'");

  std::string line;
  std::getline(ifs, line);

  std::string columnHeader;
  switch (flag) {
  case Scenario::Params::AUDITION_E0:
  case Scenario::Params::AUDITION_E1:
  case Scenario::Params::AUDITION_E2: columnHeader = "RO0"; break;
  case Scenario::Params::AUDITION_R0:
  case Scenario::Params::AUDITION_R1:
  case Scenario::Params::AUDITION_R2: columnHeader = "EO0"; break;
  default:
    utils::Thrower("Failed to assign column for flag ", flag);
  }

  auto headers = utils::split(line, ' ');
  auto it = std::find(headers.begin(), headers.end(), columnHeader);
  if (it == headers.end())
    utils::Thrower("Failed to find column ", columnHeader);
  uint col = std::distance(headers.begin(), it);

  std::vector<float> data;
  while (std::getline(ifs, line)) {
    std::stringstream ss (utils::split(line, ' ')[col]);
    float val;
    ss >> val;
    data.push_back(val);
  }

  std::cerr << "Extracted data:\n  [";
  for (float v: data) std::cerr << " " << v;
  std::cerr << "]\n";

  const auto LOOP_TIMESTEPS =
      Scenario::EVAL_STEP_DURATION * config::Simulation::ticksPerSecond() - 1;

  uint il, ir;
  for (il = 0; il<data.size() && data[il] == 0; il++);
  for (ir = std::min(il+LOOP_TIMESTEPS, uint(data.size()-1u));
       data[ir] == 0 && ir>=il;
       ir--);

  std::cerr << "Limiting auditive sample to [" << il << ":" << ir << "]\n";
  if (il == data.size() || ir == std::numeric_limits<uint>::max())
    return {NAN};

  std::vector<float> audioSample (data.begin()+il, data.begin()+ir+1);
  audioSample.push_back(0);  // Pad with a zero

  std::cerr << "Target sample:\n  [";
  for (float v: audioSample) std::cerr << " " << v;
  std::cerr << "]\n";

  return audioSample;
}

void parseNeuralEvaluationColorSpecs(const std::string &s, const std::string &a,
                                     Evaluator::Params &p) {
  using Flag = Scenario::Params::Flag;
  std::string ss = s.substr(3);

  uint cl, cr;
  Colors l_colors, r_colors;
  parseColorSpace(ss, cl, cr, l_colors, r_colors);

  std::string nbase = utils::mergeToString("ne_c", cl, cr);

  static auto colorSpec = [] (Colors colors, auto name, auto argument) {
    return Scenario::Spec{-1u, colors, argument, name};
  };

  auto stimulus = [&p, &nbase] (auto name, Colors color, auto arg, int flag) {
    if (!arg.empty() && std::isnan(arg[0])) {
      std::cerr << "Failed to extract a valid range for the audio sample\n";

    } else {
      p.specs.push_back(colorSpec(color, nbase + "_" + name, arg));
      Scenario::Params::Flags flags;
      if (flag >= 0)  flags.set(flag);
      p.flags.push_back(flags);
    }
  };

  auto vname = [] (char role, uint i) {
    return utils::mergeToString("v", role, i);
  };

  auto aname = [] (uint i, uint l, uint r) {
    return utils::mergeToString("a", i==0? 'e':'r', l, r);
  };

  auto aarg = [cl, cr] (auto arg, uint l, uint r, auto flag) {
    std::regex pattern (utils::mergeToString("c", cl, cr, "_XX"));
    std::string file = std::regex_replace(arg, pattern,
                        utils::mergeToString("c", cl, cr, "_", l, r))
                        + "/acoustics.dat";
    if (!stdfs::exists(file))
      utils::Thrower("Deduced file '", file, "' for auditive neural"
                     " evaluation does not exist");
    return parseAudioSample(file, flag);
  };

  // No audio sample
  static const decltype(Scenario::Spec::audioSample) NAS {};

  static constexpr Flag VISUAL_FLAGS [3]
    {   Flag::VISION_E0, Flag::VISION_E1, Flag::VISION_E2 };
  static constexpr Flag AUDITIVE_FLAGS [2][3]
    {{  Flag::AUDITION_E0, Flag::AUDITION_E1, Flag::AUDITION_E2 },
     {  Flag::AUDITION_R0, Flag::AUDITION_R1, Flag::AUDITION_R2 }};

  bool ok = false;
  ss = ss.substr(3 + (ss.size() > 3));
  if (ss.empty()) { // generate all
    if (a.empty())
      utils::Thrower("Missing vocalization data for auditive neural evaluation"
                     " (via scenario argument)");

    uint i=0;
    for (Color &c: r_colors)
      stimulus(vname('e', i), {c}, NAS, VISUAL_FLAGS[i]), i++;

    i=0;
    do {
      stimulus(vname('r', i++), l_colors, NAS, Flag::VISION_RECEIVER);
    } while (std::next_permutation(l_colors.begin(), l_colors.end(), rgb_cmp));

    for (int i: {0,1}) {
      uint fl = 1;
      for (uint l=2; l<=cl; l++) fl *= l;  // compute cl!
      for (uint r=0; r<cr; r++)
        for (uint l=0; l<fl; l++)
          stimulus(aname(i, l, r), {}, aarg(a, l, r, AUDITIVE_FLAGS[i][r]),
                   AUDITIVE_FLAGS[i][r]);
    }

    ok = true;

  } else if (ss.size() == 3) {
    if (ss[0] == 'v') { // parse for specific visual stimulus
      uint i = ss[2] - '0';

      switch (ss[1]) {
      case 'e':
        if (i >= cr) utils::Thrower("Invalid index for visual stimulus");
        stimulus(vname('e', i), {colors(cr)[i]}, NAS, VISUAL_FLAGS[i]);
        break;

      case 'r':
        if (i >= cl) utils::Thrower("Invalid index for visual stimulus");
        stimulus(vname('r', i), nthPermutation(colors(cl), i), NAS,
                 Flag::VISION_RECEIVER);
        break;

      default:
        utils::Thrower("Invalid case '", ss[1], "' for visual stimulus");
      }

      ok = true;
    }
  } else if (ss.size() == 4) {
    if (ss == "null") {  // null scenario (e.g. for rendering cppns/anns)
      stimulus("null", {}, NAS, -1);
      ok = true;

    } else if (ss[0] == 'a') { // parse for specific auditory stimulus
      uint role = ss[1];
      switch (role) {
      case 'e': role = 0; break;
      case 'r': role = 1; break;
      default:
        utils::Thrower("Invalid role for visual stimulus");
      }

      uint l = ss[2] - '0', r = ss[3] - '0';
      if (l >= cl || r >= cr)
        utils::Thrower("Invalid index(es) for visual stimulus");

      stimulus(aname(role, l, r), {}, aarg(a, l, r, AUDITIVE_FLAGS[role][r]),
               AUDITIVE_FLAGS[role][r]);
      ok = true;
    }
  }

  if (!ok)
    utils::Thrower("Failed to parse provided scenario spec '", s, "'");
}

void parseNeuralEvaluationSpecs (const std::string &s, const std::string &arg,
                                 Evaluator::Params &p) {
  using Type = Scenario::Type;
  p.type = Scenario::Type::NEURAL_EVALUATION;
  static constexpr auto tor = [] (Type &lhs, Type rhs) {
    return lhs = Type(uint(lhs) | uint(rhs));
  };

  switch (s[3]) {
  case 'd':
    tor(p.type, Scenario::Type::DIRECTION);
    utils::Thrower("Neural evaluation of 'Direction' scenario is not implemented");
    break;
  case 'c':
    tor(p.type, Scenario::Type::COLOR);
    parseNeuralEvaluationColorSpecs(s, arg, p);
    break;
  default:
      utils::Thrower("Unknown scenario type ", s[0], " for neural evaluation");
  }

  assert(p.flags.size() == p.specs.size());
}


bool Evaluator::Params::neuralEvaluation(const std::string &scenario) {
  return scenario.substr(0, 2) == "ne";
}

bool Evaluator::Params::neuralEvaluation(void) const {
  return Scenario::Params::neuralEvaluation(type);
}

Evaluator::Params Evaluator::Params::fromArgv (const std::string &scenario,
                                               const std::string &optarg) {
  Evaluator::Params params;

  using Type = Scenario::Type;
  params.type = Type::ERROR;
  switch (scenario[0]) {
  case 'd':
    params.type = Type::DIRECTION;
    parseDirectionSpecs(scenario, params);
    break;

  case 'c':
    params.type = Type::COLOR;
    parseColorSpecs(scenario, params);
    break;

  default:
    if (neuralEvaluation(scenario))
      parseNeuralEvaluationSpecs(scenario, optarg, params);
    else
      utils::Thrower("Unknown scenario type ", scenario[0]);
  }

  std::cout << "Parsed '" << scenario << "' into:";
  if (params.specs.size() == 1) std::cout << " " << params.specs.front();
  else {
    std::cout << "\n";
    for (const Scenario::Spec &s: params.specs)
      std::cout << "\t" << s << "\n";
  }
  std::cout << "\n";

//  std::cerr << __PRETTY_FUNCTION__ << ": Exiting immediately\n";
//  exit(0);

  return params;
}

// ===

void Evaluator::operator () (Ind &ind) {
  operator() (ind, params);
}

Evaluator::Footprint Evaluator::footprint(const Params &p) {
  size_t s =
      1 // axons cost
      + p.specs.size() * (
          4 // neurons (emitter/receiver), voice, motors energy consumption
        + 4 // Store mean/stddev for first and second half second of emitter's talk
      );
  return Footprint(s, NAN);
}

std::vector<std::string> Evaluator::footprintFields (const Params &p) {
  std::vector<std::string> v (footprint(p).size(), "UNASSIGNED");
  uint f = 0;

  v[f++] = "e_ax";

  for (uint i=0; i<p.specs.size(); i++) {
    auto l = p.scenarioParams(i).spec.name;
    v[f++] = "e_an_e_" + l;
    v[f++] = "e_an_r_" + l;
    v[f++] = "e_vc_e_" + l;
    v[f++] = "e_mt_r_" + l;

    v[f++] = utils::mergeToString("v_", l, "_m_1st");
    v[f++] = utils::mergeToString("v_", l, "_s_1st");
    v[f++] = utils::mergeToString("v_", l, "_m_lst");
    v[f++] = utils::mergeToString("v_", l, "_s_lst");
  }

  return v;
}

Scenario::Params Evaluator::Params::scenarioParams (uint i) const {
  Scenario::Params p;
  p.type = type;
  p.spec = specs[i];
  if (!flags.empty()) p.flags = flags[i];
  p.brainTemplate = nullptr;
  return p;
}

void Evaluator::operator() (Ind &ind, Params &params) {
  static const auto &TPS = config::Simulation::ticksPerSecond();
  bool brainless = true, mute = false;

//  using utils::operator<<;
//  for (int lesion: lesions) {

  uint n = params.specs.size();
  std::vector<float> scores (n);

  Footprint footprint = Evaluator::footprint(params);
  uint f = 0;

  ind.stats["stime"] = 0;

  auto start_time = Simulation::now();

  phenotype::ANN staticBrain;
  Critter::buildBrain(ind.dna, Critter::RADIUS, staticBrain);
  brainless = staticBrain.empty();

  if (false) {
    std::cerr << std::setprecision(20) << "static brain:\n";
    for (const auto &n: staticBrain.neurons()) {
      std::cerr << "\t" << n->type << " "
                << n->pos << " " << n->bias << " " << n->value << " "
                << n->depth << " " << n->flags << "\n";
      for (const auto &l: n->links())
        std::cerr << "\t\t" << l.in.lock()->pos << " " << l.weight << "\n";
    }
  }

  ind.stats["brain"] = !brainless;
  ind.stats["neurons"] = staticBrain.neurons().size()
                       - staticBrain.inputsCount() - staticBrain.outputsCount();
  ind.stats["cxts"] = staticBrain.stats().edges;
  ind.stats["depth"] = staticBrain.stats().depth;

  if (muteReceiver) ind.infos = "rmute";

  for (uint i=0; i<n && !brainless; i++) {

    Simulation simulation;
    Scenario scenario (simulation);

    auto s_params = params.scenarioParams(i);
    s_params.genome = ind.dna;
    s_params.brainTemplate = &staticBrain;
    scenario.init(s_params);
    if (muteReceiver) scenario.muteReceiver();
    auto pstr = s_params.spec.name;

    /// Modular ANN
    if (!logsSavePrefix.empty() && !annTagsFile.empty()) {
      manns.clear();

      for (simu::Critter *c: scenario.critters()) {
        auto &brain = c->brain();
        applyNeuralFlags(brain, annTagsFile);
        manns.push_back(std::make_unique<phenotype::ModularANN>(brain));

//        phenotype::ModularANN &mann = *manns.back();
//        std::cerr << "Modular ANN (" << mann.modules().size() << " items):\n";
//        for (const auto &p: mann.modules()) {
//          const phenotype::ModularANN::Module &m = *p.second;
//          std::cerr << "\tpos: " << m.center << "; " << m.neurons.size() << " neurons with flag 0x"
//                    << std::hex << m.flags << std::dec << " and type " << m.type() << "\n";
//        }
      }
    }
  //  scenario.applyLesions(lesion);

    LogData log;
    if (!logsSavePrefix.empty())
      logging_init(&log, logsSavePrefix / pstr, scenario);

    float stddev_count = 0, stddev_mean = 0, stddev_M2 = 0;
    float mean_first = 0, stddev_first = 0, mean_last = 0, stddev_last = 0;
    const auto sm_compute =
      [&stddev_count, &stddev_mean, &stddev_M2] (float &mean, float &stddev) {
      mean = stddev_mean;
      stddev = stddev_count == 0 ? 0 : sqrt(stddev_M2 / stddev_count);
    };

    const auto timestamp =
      [&simulation] { return simulation.currTime().timestamp(); };

    while (!simulation.finished() && !aborted) {
      simulation.step();

      // ====
      // = Compute running variance of emitter
      stddev_count++;
      float stddev_x = scenario.emitter()->producedSound()[1];
      float stddev_delta = stddev_x - stddev_mean;
      stddev_mean += stddev_delta / stddev_count;
      stddev_M2 += stddev_delta * (stddev_x - stddev_mean);
      if (timestamp() == TPS) {
        sm_compute(mean_first, stddev_first);
        stddev_count = stddev_mean = stddev_M2 = 0;
      }
      // ====

      // Update modules values (if modular ann is used)
      for (auto &mann: manns)
        for (const auto &p: mann->modules())
          p.second->update();

      if (!logsSavePrefix.empty()) logging_step(&log, scenario);
    }

    if (!params.neuralEvaluation()) {
      scores[i] = scenario.score();
      ind.stats["lg_" + pstr] = scores[i];
      mute |= scenario.mute();

      auto ee = scenario.emitter()->energyCosts,
           er = scenario.receiver()->energyCosts;

      if (i==0) footprint[f++] = ee[3]; // axons cost
      footprint[f++] = ee[2]; // emitter neural consumption
      footprint[f++] = er[2]; // receiver neural consumption
      footprint[f++] = ee[1]; // emitter vocal consumption
      footprint[f++] = er[0]; // receiver motor consumption

      // If aborted too quickly
      if (timestamp() < TPS)
        sm_compute(mean_first, stddev_first);
      else
        sm_compute(mean_last, stddev_last);

      // Shorted vocalisation metrics
      footprint[f++] = mean_first;
      footprint[f++] = stddev_first;
      footprint[f++] = mean_last;
      footprint[f++] = stddev_last;

      ind.stats["stime"] += Scenario::DURATION - duration(simulation);
    }
  }

  // Skip scores/novelty computations
  if (params.neuralEvaluation())  return;

  if (brainless) {
    std::replace_if(footprint.begin(), footprint.end(),
                    static_cast<bool(*)(float)>(std::isnan), 0.f);
    for (uint i=0; i<n; i++)
      ind.stats["lg_" + params.scenarioParams(i).spec.name] =
        scores[i] = Scenario::minScore();

  } else if (f != footprint.size())
    utils::Thrower("Mismatch between allocated (",
                   footprint.size(), ") and used (", f, ") footprint size");

  ind.signature = footprint;

  float totalScore = 0;
  for (float score: scores) totalScore += score;
  ind.fitnesses["lg"] = totalScore / n;

  if (n > 1)
    ind.stats["stime"] = float(ind.stats["stime"]) / n;
  ind.stats["wtime"] = Simulation::durationFrom(start_time) / 1000.f;

  ind.stats["mute"] = mute;
}

void Evaluator::dumpStats(const stdfs::path &/*dna*/,
                          const stdfs::path &/*folder*/) {
//  Simulation simulation;
//  Scenario scenario (simulation, 1);
//  Team dna_ = fromJsonFile(dna).dna;
//  scenario.init({dna_, dna_, Scenario::Params::Flags(), false, ""});

//  const simu::Critter *c = scenario.subject();
//  std::ofstream ofs (folder / "stats");
//  ofs << "bdy-mass: " << c->mass() << "\n";

//  float am = 0;
//  for (const b2Body *a: c->arms()) am += a->GetMass();
//  ofs << "arm-mass: " << am << "\n";

//  const genotype::Critter &g = c->genotype();
//  static const auto deg = [] (auto rad) { return 180 * rad / M_PI; };
//  ofs << "ang-arm0: " << deg(g.splines[0].data[genotype::Spline::SA]) << "\n";
//  ofs << "ang-arm1: " << deg(g.splines[1].data[genotype::Spline::SA]) << "\n";
//  ofs << "ang-spl2: " << deg(g.splines[2].data[genotype::Spline::SA]) << "\n";
//  ofs << "ang-spl3: " << deg(g.splines[3].data[genotype::Spline::SA]) << "\n";

//  ofs << "viw-angl: " << deg(g.vision.angleBody) << "\n";
//  ofs << "viw-prec: " << g.vision.precision << "\n";
//  ofs << "viw-widt: " << deg(g.vision.width) << "\n";

//  ofs << "clk-uppr: " << g.maxClockSpeed << "\n";
//  ofs << "clk-lowr: " << g.minClockSpeed << "\n";

//  const phenotype::ANN &b = c->brain();
//  auto nn = b.neurons().size();
//  ofs << "ann-size: " << nn << "\n";
//  ofs << "ann-hidn: " << nn - b.inputsCount() - b.outputsCount() << "\n";

//  const auto &st = b.stats();
//  ofs << "ann-dpth: " << st.depth << "\n";
//  ofs << "ann-cnxt: " << st.edges << "\n";
//  ofs << "ann-lngt: " << st.axons << "\n";

}

Evaluator::Ind Evaluator::fromJsonFile(const std::string &path) {
  std::ifstream t(path);
  if (!t) utils::Thrower("Error while opening ", path);

  std::stringstream buffer;
  buffer << t.rdbuf();

  auto o = nlohmann::json::parse(buffer.str());
  if (o.count("dna")) // assuming this is a gaga individual
    return Ind(o);
  else
    return Ind(Genome(o));
}

} // end of namespace simu
