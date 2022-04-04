message("  ###############################################################")
message("  Processing Mortal Kombat CMakeLists.txt")
set(BASE "src/experiments/mkombat/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "mortal-kombat")

############################################################################
## Target (ga evolution)
############################################################################
set(MK_SRC
    "scenario.cpp"
    "scenario.h"
    "indevaluator.cpp"
    "indevaluator.h"
)
PREPEND(MK_SRC "${BASE}")
add_library(MK_OBJS OBJECT ${MK_SRC})
add_executable(
  mk-evolver
  $<TARGET_OBJECTS:SIMU_OBJS>
  $<TARGET_OBJECTS:MK_OBJS>
  "${BASE}/evolver.cpp")
target_link_libraries(mk-evolver ${CORE_LIBS} pthread)

############################################################################
## Target (evaluation)
############################################################################
add_executable(
  mk-evaluator
  $<TARGET_OBJECTS:SIMU_OBJS>
  $<TARGET_OBJECTS:MK_OBJS>
  "${BASE}/evaluator.cpp")
target_link_libraries(mk-evaluator ${CORE_LIBS})

if (NOT CLUSTER_BUILD)
    ############################################################################
    ### Target (visualizer)
    ############################################################################

    add_executable(
        mk-visualizer
        $<TARGET_OBJECTS:SIMU_OBJS>
        $<TARGET_OBJECTS:VISU_OBJS>
        $<TARGET_OBJECTS:MK_OBJS>
        "${BASE}/visualizer.cpp"
    )
    set_property(TARGET mk-visualizer PROPERTY AUTOMOC ON)
    target_link_libraries(mk-visualizer ${CORE_LIBS} ${GUI_LIBS})
endif()
