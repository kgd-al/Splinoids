#ifndef KGD_SOUND_GENERATOR_H
#define KGD_SOUND_GENERATOR_H

#include <string>

struct RtMidiOut;

namespace kgd::watchmaker::sound {

struct StaticData {
  static constexpr uint CHANNELS = 4;
  static constexpr uint NOTES = 10;
  using NoteSheet = std::array<float, NOTES*CHANNELS>;

  static const uint TEMPO;  // BPM

  static const float NOTE_DURATION;
  static const float SONG_DURATION;

  static const uint BASE_OCTAVE;
  static const uint BASE_A;


  enum PlaybackType {
    LOOP, ONE_PASS, ONE_NOTE
  };
};

struct MidiWrapper {
  static bool initialize (std::string preferredPort = "");

  static RtMidiOut& midiOut(void);

  static void sendMessage (const std::vector<uchar> &message);

  static uchar velocity (float volume);
  static uchar key (int index);

  static void setInstrument (uchar channel, uchar i);
  static void noteOn (uchar channel, uchar note, uchar volume);
  static void notesOff (uchar channel);

  static bool writeMidi (const StaticData::NoteSheet &notes,
                         const std::string &path);
};

} // end of namespace kgd::watchmaker::sound

#endif // KGD_SOUND_GENERATOR_H
