################################################################################
## Source files (core lib)
################################################################################

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

    "enumarray.hpp"
)
PREPEND(SIMU_SRC "src/simu")

message("  Genotype: ${GNTP_SRC}")
message("Simulation: ${SIMU_SRC}")
add_library(SIMU_OBJS OBJECT ${GNTP_SRC} ${SIMU_SRC})

include_directories(box2d/include)
add_subdirectory(box2d)
list(APPEND CORE_LIBS box2d)

################################################################################
## Graphics
if (NOT CLUSTER_BUILD)
    ############################################################################
    ## Shut gcc9/Qt deprecated-copy QVariant-related warnings
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "9")
        add_compile_options(-Wno-deprecated-copy)
    endif()

    ############################################################################
    ## Sources files (gui lib)
    ############################################################################

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
endif()
