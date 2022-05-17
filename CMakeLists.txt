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
    add_subdirectory(midifile)
    list(APPEND GUI_LIBS midifile)
    include_directories(midifile/include)
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
## Experiments
################################################################################

################################################################################
## Complete ecosystem with spontaneous reproductions
##  (needs more work to be stable)
if (EXP_ECOSYSTEM)
    include(src/experiments/ecosystem/CMakeLists.txt)
endif()

################################################################################
## Life-dinner (Discrimination, motion)
if (EXP_LIFEDINNER)
    include(src/experiments/life-dinner/CMakeLists.txt)
endif()

################################################################################
## Mortal Kombat (morphogenesis, competition, communication)
if (EXP_KOMBAT)
    include(src/experiments/mkombat/CMakeLists.txt)
endif()

################################################################################
## Language (communication, phonemes)
if (EXP_LANGUAGE)
    include(src/experiments/language/CMakeLists.txt)
endif()

################################################################################
## Toy projects
################################################################################

if (NOT CLUSTER_BUILD)
    ############################################################################
    ## Song-maker (ANN-based sound production, watchmaker)
    if(WITH_MIDI AND TOYS_SONGMAKER)
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

set(CRITTER_SPLINES 4 CACHE STRING "Number of splines on each side")
add_definitions(-DNUMBER_OF_SPLINES=${CRITTER_SPLINES})
message("> Splines/critter: " ${CRITTER_SPLINES})

set(CRITTER_ARMS 1 CACHE STRING "Number of arms on each side")
add_definitions(-DNUMBER_OF_ARMS=${CRITTER_ARMS})
message(">    Arms/critter: " ${CRITTER_ARMS})

set(CRITTER_ARM_PARTS 2 CACHE STRING "Number of splines per arm")
add_definitions(-DNUMBER_OF_ARM_PARTS=${CRITTER_ARM_PARTS})
message(">    Arm sections: " ${CRITTER_ARM_PARTS})

message("< Building targets >")

option(TOYS_SHAPEMAKER "Whether or not to build morphological watchmaker" OFF)
message(">    Shapemaker: " ${TOYS_SHAPEMAKER})

option(TOYS_SONGMAKER "Whether or not to build songmaker" OFF)
message(">     Songmaker: " ${TOYS_SONGMAKER})

option(EXP_ECOSYSTEM "Whether or not to build these targets" OFF)
message(">     Ecosystem: " ${EXP_ECOSYSTEM})

option(EXP_LIFEDINNER "Whether or not to build these targets" OFF)
message(">   Life/Dinner: " ${EXP_LIFEDINNER})

option(EXP_KOMBAT "Whether or not to build these targets" OFF)
message("> Mortal Kombat: " ${EXP_KOMBAT})

option(EXP_LANGUAGE "Whether or not to build these targets" OFF)
message(">      Language: " ${EXP_LANGUAGE})

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
