#include "config.h"
#include "sound/visualizer.h"

namespace config {
#define CFILE WatchMaker
using namespace kgd::watchmaker;

DEFINE_PARAMETER(TemporalInput, tinput, TemporalInput::NONE)
DEFINE_PARAMETER(Audition, audition, Audition::SELF)

DEFINE_PARAMETER(int, dataLogLevel, 1)

using D = kgd::watchmaker::sound::StaticData;
DEFINE_CONST_PARAMETER(uint, songChannels, D::CHANNELS)
DEFINE_CONST_PARAMETER(uint, songBaseOctave, D::BASE_OCTAVE)
DEFINE_CONST_PARAMETER(uint, songTempo, D::TEMPO)
DEFINE_CONST_PARAMETER(uint, songNotes, D::NOTES)
DEFINE_CONST_PARAMETER(uint, songDuration, D::SONG_DURATION)

DEFINE_PARAMETER(std::string, midiPort, "Timidity")
DEFINE_PARAMETER(int, midiInstrument, 0)
DEFINE_PARAMETER(bool, polyphonic, true)

DEFINE_SUBCONFIG(config::EvolvableSubstrate, configESHN)

#undef CFILE
} // end of namespace config
