#include <csignal>

#include "indevaluator.h"

#include "kgd/external/cxxopts.hpp"

//#include "config/dependencies.h"

int main(int argc, char *argv[]) {
  // ===========================================================================
  // == Command line arguments parsing

  using CGenome = genotype::Critter;

  std::string arg;
  std::vector<std::string> positionals;

  cxxopts::Options options("Splinoids (mk-team-tool)",
                           "Team <-> Genomes transformation tool");
  options.add_options()
    ("h,help", "Display help")
    ("c,create", "Create a team from the remaining arguments",
     cxxopts::value(arg))
    ("x,extract", "Extracts genomes from provided team",
     cxxopts::value(arg))
    ("positional", "Positional parameters",
      cxxopts::value<std::vector<std::string>>(positionals))
    ;
  options.parse_positional({"positional"});

  auto result = options.parse(argc, argv);

  if (result.count("help")) {
    std::cout << options.help()
              << std::endl;
    return 0;
  }

  bool c = result.count("create"), x = result.count("extract");
  if (!(c || x)) {
    std::cerr << "No create/extract options provided\n";
    return 1;
  }

  if (c && x) {
    std::cerr << "Incompatible create/extract options\n";
    return 1;
  }

  using utils::operator <<;
  if (c)
    std::cout << arg << " < " << positionals << "\n";
  else
    std::cout << arg << " >>\n";

  if (c) {
    if (stdfs::exists(arg)) {
      std::cerr << "Output team file " << arg
                << " already exists. I shall NOT overwrite it!\n";
      return 2;
    }

    simu::Team team;
    for (const std::string &f: positionals) {
      if (!stdfs::exists(f)) {
        std::cerr << "Genome file " << f << " does not exist!\n";
        return 3;
      }
      team.members.push_back(CGenome::fromFile(f));
    }

    team.toFile(arg);

  } else if (x) {
    stdfs::path base = arg;
    base.replace_extension("");

    simu::Team team = simu::Team::fromFile(arg);
    uint i = 0;
    for (const CGenome &g: team.members) {
      std::ostringstream oss;
      oss << base.string() << "_" << i++;
      g.toFile(oss.str());
    }
  }

  return 0;
}
