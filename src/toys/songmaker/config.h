#ifndef KGD_WATCHMAKER_CONFIG_H
#define KGD_WATCHMAKER_CONFIG_H

#include "kgd/settings/configfile.h"
#include "../phenotype/ann.h"

DEFINE_NAMESPACE_SCOPED_PRETTY_ENUMERATION(
    kgd::watchmaker, TemporalInput,
        NONE, LINEAR, SINUSOIDAL)

DEFINE_NAMESPACE_SCOPED_PRETTY_ENUMERATION(
    kgd::watchmaker, Audition,
        NONE, SELF, COMMUNITY)

namespace config {

struct CONFIG_FILE(WatchMaker) {
  DECLARE_SUBCONFIG(config::EvolvableSubstrate, configESHN)

  DECLARE_PARAMETER(kgd::watchmaker::TemporalInput, tinput)
  DECLARE_PARAMETER(kgd::watchmaker::Audition, audition)

  DECLARE_CONST_PARAMETER(uint, songChannels)
  DECLARE_CONST_PARAMETER(uint, songBaseOctave)
  DECLARE_CONST_PARAMETER(uint, songTempo)
  DECLARE_CONST_PARAMETER(uint, songNotes)
  DECLARE_CONST_PARAMETER(uint, songDuration)

  DECLARE_PARAMETER(std::string, midiPort)
  DECLARE_PARAMETER(int, midiInstrument)
  DECLARE_PARAMETER(bool, polyphonic)

  DECLARE_PARAMETER(int, dataLogLevel)
};

} // end of namespace config

#endif // KGD_WATCHMAKER_CONFIG_H
