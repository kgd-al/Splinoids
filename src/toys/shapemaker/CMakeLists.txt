set(SOUND_SRC
    "generator.cpp"
    "visualizer.cpp"

    "rtmidi/RtMidi.cpp"
)
PREPEND(SOUND_SRC "sound")
set(MIDI_DEFS __LINUX_ALSA__)
set(MIDI_LIBS asound pthread)

add_subdirectory(midi/midifile/)
list(APPEND MIDI_LIBS midifile)

set(WATCHMAKER_SRC
    ${SOUND_SRC}
    "qflowlayout.cpp"
    "config.cpp"
    "bwwindow.cpp"
    "main.cpp"
)
PREPEND(WATCHMAKER_SRC "src/watchmaker")

add_executable(
  watchmaker
  ${WATCHMAKER_SRC}
)
set_property(TARGET watchmaker PROPERTY AUTOMOC ON)
target_link_libraries(watchmaker eshn-core eshn-gui
                                 ${CORE_LIBS} ${GUI_LIBS} ${MIDI_LIBS})

target_compile_definitions(watchmaker PRIVATE ${MIDI_DEFS})
