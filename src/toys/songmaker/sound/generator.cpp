#include <cmath>

#include <QFile>
#include <QDataStream>

#include "generator.h"
#include "../config.h"

#define __RTMIDI_DEBUG__
#include "rtmidi/RtMidi.h"

#include <QDebug>
#include <iomanip>

#include <fstream>
#include <sstream>
#include <cassert>
#include <QDateTime>
#include <QTimer>

#ifndef NDEBUG
#define DEBUG_AUDIO 0
#define DEBUG_MIDI_AUDIO 100
#endif

#include "../../../midi/midifile/include/MidiFile.h"

namespace kgd::watchmaker::sound {

const uint StaticData::TEMPO = 120;  // BPM

const float StaticData::NOTE_DURATION = 60.f / StaticData::TEMPO;
const float StaticData::SONG_DURATION =
    StaticData::NOTES * 60.f / StaticData::TEMPO;

const uint StaticData::BASE_OCTAVE = 1;
const uint StaticData::BASE_A = 21 + 12 * StaticData::BASE_OCTAVE;

uchar MidiWrapper::key (int index) {   return StaticData::BASE_A+12*index;  }
uchar MidiWrapper::velocity (float volume) {
  assert(-1 <= volume && volume <= 1);
  return std::round(127*std::max(0.f, volume));
}

RtMidiOut& MidiWrapper::midiOut (void) {
  static RtMidiOut *o = new RtMidiOut(RtMidi::LINUX_ALSA);
  return *o;
}

bool MidiWrapper::initialize (std::string preferredPort) {
  if (preferredPort.empty())  preferredPort = config::WatchMaker::midiPort();

  auto &midi = midiOut();
  uint n = midi.getPortCount();
  int index = -1;
  std::cout << "\nPreferred midi port: " << preferredPort << "\n";
  std::cout << "\t" << n << " output ports available.\n";
  for (uint i=0; i<n; i++) {
    std::string portName = midi.getPortName(i);
    bool match = false;
    if (index < 0) {
      auto it = std::search(portName.begin(), portName.end(),
                            preferredPort.begin(), preferredPort.end(),
                            [] (char lhs, char rhs) {
        return std::toupper(lhs) == std::toupper(rhs);
      });
      match = (it != portName.end());
      if (match)  index = i;
    }
    std::cout << (match? '*' : ' ') << " Output Port #" << i+1 << ": "
              << portName << '\n';
  }
  std::cout << '\n';

  if (index < 0) {
    std::cout << "Did not find a port matching pattern '" << preferredPort
              << "'.\nDefaulting to first available port\n";
    index = 0;
  }

  std::cout << "Using port #" << index+1 << ": " << midi.getPortName(index)
            << std::endl;
  midi.openPort(index);

  return true;
}

void MidiWrapper::sendMessage (const std::vector<uchar> &message) {
  std::cout << "Sending Midi message:";
  for (uchar c: message)
    std::cout << " " << std::hex << std::setw(2) << uint(c);
  std::cout << std::endl;

  midiOut().sendMessage(&message);
}

void MidiWrapper::setInstrument(uchar channel, uchar i) {
  static std::vector<uchar> buffer ({0x90, 0x00});
  buffer[0] = 0xC0 | (channel & 0x0F);
  buffer[1] = i & 0x7F;
  sendMessage(buffer);
}

void MidiWrapper::noteOn(uchar channel, uchar note, uchar volume) {
  static std::vector<uchar> buffer ({0x90, 0x00, 0x00});
  buffer[0] = 0x90 | (channel & 0x0F);
  buffer[1] = note & 0x7F;
  buffer[2] = volume & 0x7F;
  sendMessage(buffer);
}

void MidiWrapper::notesOff(uchar channel) {
  static std::vector<uchar> buffer ({0x90, 0x00, 0x00});
  buffer[0] = 0xB0 | (channel & 0x0F);
  buffer[1] = 0x7B;
  sendMessage(buffer);
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value, void>::type
write (std::ostream &os, T value) {
  static constexpr auto S = sizeof(T);
  static_assert(S >= 1, "Doesn't make any sense");

  union { std::array<uint8_t, S> array; T value; } data;
  data.value = value;

  for (auto it=data.array.rbegin(); it != data.array.rend(); ++it)
    os << *it;
}

bool MidiWrapper::writeMidi(const StaticData::NoteSheet &notes,
                            const std::string &path) {

  static constexpr auto N = StaticData::NOTES;
  static constexpr auto C = StaticData::CHANNELS;
  static constexpr auto T = StaticData::TEMPO;

  static constexpr auto tpq = 96;

  static std::vector<uchar> notesOff { 0xB0, 0x7B, 0x00 };

  {
    stdfs::path kgd_path = path;
    kgd_path.replace_extension(".kgd.mid");
    std::ofstream ofs (kgd_path, std::ios::out | std::ios::binary);
    if (!ofs) return false;

    // header
    ofs << "MThd";                    // Identifier
    write<uint32_t>(ofs, 0x00000006); // Chunklen (fixed to 6)
    write<uint16_t>(ofs, 0x0000);     // Format 0 -> single track midi
    write<uint16_t>(ofs, 0x0001);     // 1 track
    write<uint16_t>(ofs, tpq);        // ppqn/tpq

    // data
    ofs << "MTrk";                    // Identifier
    write<uint32_t>(ofs, 0x00000063); // Track length (filled in latter)

    // Set tempo (at time 0)
    int tempo_us = int(60.f / T * 1000000.f + .5f);
    write<uint32_t>(ofs, 0x00ff5103);
    write<uint16_t>(ofs, tempo_us >>8);
    write<uint8_t>(ofs, (uint8_t)(tempo_us & 0xFF));

    write<uint8_t>(ofs, 0x00);  // Instrument change at time 0
    write<uint16_t>(ofs, 0xC000 | config::WatchMaker::midiInstrument());

    uint32_t prev = 0;
    std::array<bool, C> on {false};

    const auto writeDt = [&ofs, &prev] (uint32_t t) {
      uint32_t dt = t - prev;
      prev = t;

      uint8_t bytes [4] {
        (uint8_t)(((uint32_t)dt >> 21) & 0x7f),  // most significant 7 bits
        (uint8_t)(((uint32_t)dt >> 14) & 0x7f),
        (uint8_t)(((uint32_t)dt >> 7)  & 0x7f),
        (uint8_t)(((uint32_t)dt)       & 0x7f),  // least significant 7 bits
      };

      int start = 0;
      while ((start<4) && (bytes[start] == 0))  start++;

      for (int i=start; i<3; i++) {
        bytes[i] = bytes[i] | 0x80;
        write(ofs, bytes[i]);
      }
      write(ofs, bytes[3]);
    };
    const auto noteEvent =
      [writeDt, &ofs, &prev] (uint t, uint8_t note, uint8_t velocity, bool on) {
      writeDt(t);
      write(ofs, uint8_t(on ? 0x90 : 0x80));
      write(ofs, note);
      write(ofs, velocity);
    };

    for (uint n=0; n<N; n++) {
      int t = tpq * n;

      for (uint c=0; c<C; c++) {
        float fn = notes[c+C*n];
        uchar cn = velocity(fn);

        if (n == 0 || velocity(notes[c+C*(n-1)]) != cn) {
          if (on[c]) {
            noteEvent(t, key(c), 0, false);
            on[c] = false;
          }

          if (cn > 0) {
            noteEvent(t, key(c), cn, true);
            on[c] = true;
          }
        }
      }
    }

    // All notes off
    writeDt(tpq * N);
    write<uint16_t>(ofs, 0xB07B);
    write<uint8_t>(ofs, 0x00);

    // End of track
    writeDt(tpq * N);
    write<uint16_t>(ofs, 0xFF2F);
    write<uint8_t>(ofs, 0x00);

    // Write track size back into header
    uint32_t size = ofs.tellp();
    size -= 22;
    ofs.seekp(18, std::ios::beg);
    write(ofs, size);

  }{
    smf::MidiFile midifile;
    midifile.setTicksPerQuarterNote(tpq);

    midifile.addTempo(0, 0, T);
    midifile.addTimbre(0, 0, 0, config::WatchMaker::midiInstrument());

    std::array<bool, C> on {false};
    for (uint n=0; n<N; n++) {
      int t = tpq * n;  /// TODO Debug
  //    std::cerr << "t(" << n << ") = " << t << "\n";
      for (uint c=0; c<C; c++) {
        float fn = notes[c+C*n];
        uchar cn = velocity(fn);
        if (n == 0 || velocity(notes[c+C*(n-1)]) != cn) {
          if (on[c] > 0) {
            midifile.addNoteOff(0, t, 0, key(c), 0);
            on[c] = false;
          }

          if (cn > 0) {
            midifile.addNoteOn(0, t, 0, key(c), cn);
            on[c] = true;
          }
        }
      }
    }

  //  for (uint c=0; c<C; c++)
  //    if (on[c])
  //      midifile.addNoteOff(0, tpq * N, 0, A+c, 0);
    midifile.addEvent(0, tpq * N, notesOff);

//    midifile.sortTracks();

    midifile.doTimeAnalysis();

    stdfs::path midifile_path = path;
    midifile_path.replace_extension(".midifile.mid");
    midifile.write(midifile_path);
  }

  return true;
}

} // end of namespace kgd::watchmaker::sound
