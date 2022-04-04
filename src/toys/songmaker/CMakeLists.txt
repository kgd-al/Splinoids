set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "toys")
message("CMAKE_RUNTIME_OUTPUT_DIRECTORY: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}")
message("RUNTIME_OUTPUT_DIRECTORY: ${RUNTIME_OUTPUT_DIRECTORY_RELEASE}")
set(SOUND_SRC
    "generator.cpp"
    "visualizer.cpp"

    "rtmidi/RtMidi.cpp"
)
PREPEND(SOUND_SRC "sound")
set(MIDI_DEFS __LINUX_ALSA__)
set(MIDI_LIBS asound pthread)

set(SONGMAKER_SRC
    ${SOUND_SRC}
    "qflowlayout.cpp"
    "config.cpp"
    "bwwindow.cpp"
    "main.cpp"
)
PREPEND(SONGMAKER_SRC "src/toys/songmaker")

add_executable(songmaker ${SONGMAKER_SRC})
set_property(TARGET songmaker PROPERTY AUTOMOC ON)
target_link_libraries(songmaker ${CORE_LIBS} ${GUI_LIBS} ${MIDI_LIBS})

target_compile_definitions(songmaker PRIVATE ${MIDI_DEFS})
