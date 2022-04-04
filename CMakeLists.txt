cmake_minimum_required(VERSION 3.4)

################################################################################
## General settings
################################################################################


project("Splinoids")
message("\n####################################################################"
    "############\n## CMakeFile for ${PROJECT_NAME}")

set(CMAKE_CXX_FLAGS "-Wall -Wextra -pedantic")#"-ftime-report")

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")


option(WITH_DEBUG_INFO "Sets whether to maximize debug data collection" OFF)
message("> With debug info " ${WITH_DEBUG_INFO})
if(WITH_DEBUG_INFO)
    add_definitions(-DWITH_DEBUG_INFO)

    # Enable small memory error detector (fuse=gold fixes linker errors)
#    set(ASAN "-fsanitize=thread")
    set(ASAN "-fsanitize=address -fsanitize=undefined \
        -fno-sanitize-recover=all")
    string(APPEND CMAKE_CXX_FLAGS " -g ${ASAN} -fno-omit-frame-pointer")
    message("ASAN: ${ASAN}")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        string(APPEND CMAKE_CXX_FLAGS " -fuse-ld=gold")
    endif()
endif()

################################################################################
## Multi-configuration installation path
################################################################################

if (${CMAKE_INSTALL_PREFIX} MATCHES "^$ENV{HOME}")
    string(TOLOWER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
    string(APPEND CMAKE_INSTALL_PREFIX "/${BUILD_TYPE}")
    set(CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX})
endif()
message("> Install path is ${CMAKE_INSTALL_PREFIX}/")


################################################################################
## Managing uneven support of std 17 filesystem
################################################################################


if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  link_directories(/usr/local/opt/llvm/lib/)
endif()


################################################################################
## Dependencies
################################################################################

find_package(ES-HyperNEAT REQUIRED)
include_directories(${ES-HyperNEAT_INCLUDE_DIRS})
message("> ES-HyperNEAT found at " ${ES-HyperNEAT_DIR})
list(APPEND CORE_LIBS ${ES-HyperNEAT_LIBRARIES})
message("  > Core libraries " "${CORE_LIBS}")
list(APPEND GUI_LIBS ${ES-HyperNEAT_GUI_LIBRARIES})
message("  > Gui libraries " "${GUI_LIBS}")

list(APPEND KGD_DEFINITIONS ${ES-HyperNEAT_KGD_DEFINITIONS})

if (WITH_MIDI)
    add_subdirectory(midi/midifile)
    list(APPEND GUI_LIBS midifile)
    include_directories(midi/midifile/include)
endif()

################################################################################
## Process additional pre-processor definitions
################################################################################
add_definitions(${KGD_DEFINITIONS})
message("Using additionnal pre-processor definitions: ${KGD_DEFINITIONS}")

################################################################################
## Subfolders helper
################################################################################

FUNCTION(PREPEND output prefix)
   SET(listVar "")
   FOREACH(f ${${output}})
      LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${output} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

################################################################################
## Base libraries (core + gui)
################################################################################

include(src/CMakeLists.txt)

################################################################################
## ?
################################################################################

if (EXP_ECOSYSTEM)
    ############################################################################
    ## Target (simulation)
    ############################################################################
    add_executable(
      simulator
      $<TARGET_OBJECTS:SIMU_OBJS>
      "src/simulator.cpp")
    target_link_libraries(simulator ${CORE_LIBS})

    ############################################################################
    ## Target (evaluation)
    ############################################################################
    #add_executable(
    #  evaluator
    #  $<TARGET_OBJECTS:SIMU_OBJS>
    #  "src/evaluator.cpp")
    #target_link_libraries(evaluator ${CORE_LIBS})

    ############################################################################
    ## Target (timelines explorer)
    ############################################################################
    find_package(OpenMP)
    #if (OPENMP_FOUND)
    #    message("> OpenMP Found.")
    #    message("  > OpenMP flags are " ${OpenMP_CXX_LIBRARIES})
    #  add_executable(
    #    timelines
    #    $<TARGET_OBJECTS:SIMU_OBJS>
    #    "src/timelines.cpp")
    #  target_link_libraries(timelines ${CORE_LIBS} OpenMP::OpenMP_CXX)
    #endif()


    ################################################################################
    ## Target (tester may be with gui)
    ################################################################################
    #if (CLUSTER_BUILD)
    #    add_executable(
    #        tester
    #        $<TARGET_OBJECTS:SIMU_OBJS>
    #        "src/tests/tester.cpp")
    #    target_link_libraries(tester ${CORE_LIBS})
    #else()
    #    add_executable(
    #        tester
    #        $<TARGET_OBJECTS:SIMU_OBJS>
    #        $<TARGET_OBJECTS:VISU_OBJS>
    #        "src/tests/tester.cpp")
    #    target_link_libraries(tester ${CORE_LIBS} ${GUI_LIBS})
    #endif()

endif()


################################################################################
## Experiments
################################################################################

################################################################################
## Life-dinner (Discrimination, motion)
if (EXP_LIFE-DINNER)
    include(src/experiments/life-dinner/CMakeLists.txt)
endif()

################################################################################
## Mortal Kombat (morphogenesis, competition, communication)
if (EXP_KOMBAT)
    include(src/experiments/mkombat/CMakeLists.txt)
endif()

################################################################################
## Toy projects
################################################################################

if (NOT CLUSTER_BUILD)
    ############################################################################
    ## Song-maker (ANN-based sound production, watchmaker)
    if (TOYS_SONGMAKER)
        include(src/toys/songmaker/CMakeLists.txt)
    endif()

    ############################################################################
    ## Shape-maker (Spline-based morphology, watchmaker)
    if (TOYS_SHAPEMAKER)
        include(src/toys/shapemaker/CMakeLists.txt)
    endif()

endif()

################################################################################
### Additional flags
################################################################################


#message("")

option(CLUSTER_BUILD "Whether or not building on a cluster
                      (i-e no gui and local linkage)" ON)
message("> Cluster building mode is " ${CLUSTER_BUILD})
if(CLUSTER_BUILD)
    add_definitions(-DCLUSTER_BUILD)
endif()

option(USE_DIMORPHISM "Whether or not model sexual dimorphism" ON)
message("> Sexual dimorphism is " ${USE_DIMORPHISM})
if(USE_DIMORPHISM)
    add_definitions(-DUSE_DIMORPHISM)
endif()

option(TOYS_SHAPEMAKER "Whether or not to build morphological watchmaker" OFF)
message("> Building Shapemaker target: " ${TOYS_SHAPEMAKER})

option(TOYS_SONGMAKER "Whether or not to build songmaker" OFF)
message("> Building Songmaker target: " ${TOYS_SONGMAKER})

option(EXP_ECOSYSTEM "Whether or not to build these targets" OFF)
message("> Building Ecosystem targets: " ${EXP_ECOSYSTEM})

option(EXP_ALIFE2021 "Whether or not to build these targets" OFF)
message("> Building ALife2021 targets: " ${EXP_ALIFE2021})

option(EXP_KOMBAT "Whether or not to build these targets" OFF)
message("> Building Mortal Kombat targets: " ${EXP_KOMBAT})

option(WITH_MIDI "Whether or not use midifile library" OFF)
message("> Midi support is " ${WITH_MIDI})
if(WITH_MIDI)
    add_definitions(-DWITH_MIDI)
endif()

#option(UPDATE_DEPENDENCIES "Whether or not to keep dependency data up-to-date
#                            (implies extra compilation time)" ON)
#message("> Auto updating dependencies " ${UPDATE_DEPENDENCIES})

#if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
#    option(DEBUG_COLLISIONS "Add CPU/RAM cost to view collision tests" OFF)
#    message("> Debugging collisions: " ${DEBUG_COLLISIONS})
#    if (DEBUG_COLLISIONS)
#        add_definitions(-DDEBUG_COLLISIONS)
#    endif()
#endif()

#message("> Build type is " ${CMAKE_BUILD_TYPE})
#message("> Compile flags are " ${CMAKE_CXX_FLAGS})
#message("")
