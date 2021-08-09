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

################################################################################
## Process additional pre-processor definitions
################################################################################
add_definitions(${KGD_DEFINITIONS})
message("Using additionnal pre-processor definitions: ${KGD_DEFINITIONS}")

################################################################################
## Source files (simulation)
################################################################################

FUNCTION(PREPEND output prefix)
   SET(listVar "")
   FOREACH(f ${${output}})
      LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${output} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)

set(GNTP_SRC
    "config.h"
    "critter.h"
    "critter.cpp"
    "environment.h"
    "environment.cpp"
)
PREPEND(GNTP_SRC "src/genotype")

set(SIMU_SRC
    "critter.h"
    "critter.cpp"
    "foodlet.h"
    "foodlet.cpp"
    "environment.h"
    "environment.cpp"
    "simulation.h"
    "simulation.cpp"
    "box2dutils.h"
    "box2dutils.cpp"
    "config.h"
    "config.cpp"
    "time.h"
    "time.cpp"

    "../enumarray.hpp"
)
PREPEND(SIMU_SRC "src/simu")

add_library(SIMU_OBJS OBJECT ${GNTP_SRC} ${SIMU_SRC})

include_directories(box2d/include)
add_subdirectory(box2d)

list(APPEND CORE_LIBS box2d)

################################################################################
## Target (simulation)
################################################################################
add_executable(
  simulator
  $<TARGET_OBJECTS:SIMU_OBJS>
  "src/simulator.cpp")
target_link_libraries(simulator ${CORE_LIBS})

################################################################################
## Target (evaluation)
################################################################################
#add_executable(
#  evaluator
#  $<TARGET_OBJECTS:SIMU_OBJS>
#  "src/evaluator.cpp")
#target_link_libraries(evaluator ${CORE_LIBS})

################################################################################
## Target (timelines explorer)
################################################################################
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

if (EXP_ALIFE2021)
    ############################################################################
    ## Target (alife2021 ga evolution)
    ############################################################################
    set(PP_SRC
        "scenario.cpp"
        "scenario.h"
        "indevaluator.cpp"
        "indevaluator.h"
    )
    PREPEND(PP_SRC "src/alife2021")
    add_library(PP_OBJS OBJECT ${PP_SRC})
    add_executable(
      pp-evolver
      $<TARGET_OBJECTS:SIMU_OBJS>
      $<TARGET_OBJECTS:PP_OBJS>
      "src/alife2021/evolver.cpp")
    target_link_libraries(pp-evolver ${CORE_LIBS} pthread)

    ############################################################################
    ## Target (alife2021 evaluation)
    ############################################################################
    add_executable(
      pp-evaluator
      $<TARGET_OBJECTS:SIMU_OBJS>
      $<TARGET_OBJECTS:PP_OBJS>
      "src/alife2021/evaluator.cpp")
    target_link_libraries(pp-evaluator ${CORE_LIBS})
endif()

if (EXP_KOMBAT)
    ############################################################################
    ## Target (Mortal Kombat ga evolution)
    ############################################################################
    set(MK_SRC
        "scenario.cpp"
        "scenario.h"
        "indevaluator.cpp"
        "indevaluator.h"
    )
    PREPEND(MK_SRC "src/mkombat")
    add_library(MK_OBJS OBJECT ${MK_SRC})
    add_executable(
      mk-evolver
      $<TARGET_OBJECTS:SIMU_OBJS>
      $<TARGET_OBJECTS:MK_OBJS>
      "src/mkombat/evolver.cpp")
    target_link_libraries(mk-evolver ${CORE_LIBS} pthread)

    ############################################################################
    ## Target (Mortal Kombat evaluation)
    ############################################################################
    add_executable(
      mk-evaluator
      $<TARGET_OBJECTS:SIMU_OBJS>
      $<TARGET_OBJECTS:MK_OBJS>
      "src/mkombat/evaluator.cpp")
    target_link_libraries(mk-evaluator ${CORE_LIBS})
endif()

################################################################################
## Graphics
if (NOT CLUSTER_BUILD)
    ############################################################################
    ## Shut gcc9/Qt deprecated-copy QVariant-related warnings
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "9")
        add_compile_options(-Wno-deprecated-copy)
    endif()

################################################################################
## Sources files (visualizer)
################################################################################

    set(VISU_SRC
        "critter.h"
        "critter.cpp"
        "foodlet.h"
        "foodlet.cpp"
        "obstacle.h"
        "obstacle.cpp"
        "environment.h"
        "environment.cpp"
        "graphicsimulation.h"
        "graphicsimulation.cpp"
        "config.h"
        "config.cpp"

        "b2debugdrawer.h"
        "b2debugdrawer.cpp"
    )
    PREPEND(VISU_SRC "src/visu")

    set(GUI_SRC
        "mainview.h"
        "mainview.cpp"
        "actionsbuilder.hpp"
        "geneticmanipulator.h"
        "geneticmanipulator.cpp"
        "statsview.h"
        "statsview.cpp"
        "joystick.h"
        "joystick.cpp"
    )

    PREPEND(GUI_SRC "src/gui")

    set(GUI_MISC_SRC
        "joystick/joystick.hh"
        "joystick/joystick.cc"

        "qt-collapsible-section/Section.h"
        "qt-collapsible-section/Section.cpp"
    )

    add_library(VISU_OBJS OBJECT ${VISU_SRC} ${GUI_SRC} ${GUI_MISC_SRC})
    target_include_directories(VISU_OBJS PRIVATE
        $<TARGET_PROPERTY:Qt5::Widgets,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(VISU_OBJS PRIVATE
        $<TARGET_PROPERTY:Qt5::Widgets,INTERFACE_COMPILE_DEFINITIONS>)
    set_property(TARGET VISU_OBJS PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET VISU_OBJS PROPERTY AUTOMOC ON)


################################################################################
### Target (visualizer)
################################################################################

    add_executable(
      visualizer
      $<TARGET_OBJECTS:SIMU_OBJS>
      $<TARGET_OBJECTS:VISU_OBJS>
      "src/visualizer.cpp"
    )
    set_property(TARGET visualizer PROPERTY AUTOMOC ON)
    target_link_libraries(visualizer ${CORE_LIBS} ${GUI_LIBS})

################################################################################
### Targets (Watchmaker)
################################################################################

    if (EXP_WATCHMAKER)
        add_executable(
          watchmaker
          $<TARGET_OBJECTS:SIMU_OBJS>
          $<TARGET_OBJECTS:VISU_OBJS>
          "src/misc/watchmaker_classes.h"
          "src/misc/watchmaker_classes.cpp"
          "src/watchmaker.cpp"
        )
        set_property(TARGET watchmaker PROPERTY AUTOMOC ON)
        target_link_libraries(watchmaker ${CORE_LIBS} ${GUI_LIBS})
    endif()

    if (EXP_ALIFE2021)
    ############################################################################
    ### Targets (alife2021 visualizer)
    ############################################################################

        add_executable(
            pp-visualizer
            $<TARGET_OBJECTS:SIMU_OBJS>
            $<TARGET_OBJECTS:VISU_OBJS>
            $<TARGET_OBJECTS:PP_OBJS>
            "src/alife2021/visualizer.cpp"
        )
        set_property(TARGET pp-visualizer PROPERTY AUTOMOC ON)
        target_link_libraries(pp-visualizer ${CORE_LIBS} ${GUI_LIBS})
    endif()

    if (EXP_KOMBAT)
    ############################################################################
    ### Targets (mortal kombat visualizer)
    ############################################################################

        add_executable(
            mk-visualizer
            $<TARGET_OBJECTS:SIMU_OBJS>
            $<TARGET_OBJECTS:VISU_OBJS>
            $<TARGET_OBJECTS:MK_OBJS>
            "src/mkombat/visualizer.cpp"
        )
        set_property(TARGET mk-visualizer PROPERTY AUTOMOC ON)
        target_link_libraries(mk-visualizer ${CORE_LIBS} ${GUI_LIBS})
    endif()

################################################################################
### Targets (Visu tool)
################################################################################

#    add_executable(
#        visu-tool
#        $<TARGET_OBJECTS:SIMU_OBJS>
#        $<TARGET_OBJECTS:VISU_OBJS>
#        "src/visualuator.cpp"
#    )
#    set_property(TARGET visu-tool PROPERTY AUTOMOC ON)
#    target_link_libraries(visu-tool ${CORE_LIBS} ${GUI_LIBS})

endif()

################################################################################
## Target (tester may be with gui)
################################################################################
if (CLUSTER_BUILD)
    add_executable(
        tester
        $<TARGET_OBJECTS:SIMU_OBJS>
        "src/tests/tester.cpp")
    target_link_libraries(tester ${CORE_LIBS})
else()
    add_executable(
        tester
        $<TARGET_OBJECTS:SIMU_OBJS>
        $<TARGET_OBJECTS:VISU_OBJS>
        "src/tests/tester.cpp")
    target_link_libraries(tester ${CORE_LIBS} ${GUI_LIBS})
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
if (EXP_ALIFE2021)
    ############################################################################
    ## Target (alife2021 ga evolution)
    ############################################################################
    set(PP_SRC
        "scenario.cpp"
        "scenario.h"
        "indevaluator.cpp"
        "indevaluator.h"
    )
    PREPEND(PP_SRC "src/alife2021")
    add_library(PP_OBJS OBJECT ${PP_SRC})
    add_executable(
      pp-evolver
      $<TARGET_OBJECTS:SIMU_OBJS>
      $<TARGET_OBJECTS:PP_OBJS>
      "src/alife2021/evolver.cpp")
    target_link_libraries(pp-evolver ${CORE_LIBS} pthread)

    ############################################################################
    ## Target (alife2021 evaluation)
    ############################################################################
    add_executable(
      pp-evaluator
      $<TARGET_OBJECTS:SIMU_OBJS>
      $<TARGET_OBJECTS:PP_OBJS>
      "src/alife2021/evaluator.cpp")
    target_link_libraries(pp-evaluator ${CORE_LIBS})
endif()

option(EXP_WATCHMAKER "Whether or not to build these targets" OFF)
message("> Building watchamker target: " ${EXP_WATCHMAKER})

option(EXP_ALIFE2021 "Whether or not to build these targets" OFF)
message("> Building ALife2021 targets: " ${EXP_ALIFE2021})

option(EXP_KOMBAT "Whether or not to build these targets" OFF)
message("> Building Mortal Kombat targets: " ${EXP_KOMBAT})

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
