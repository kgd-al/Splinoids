message("  ###############################################################")
message("  Processing Language CMakeLists.txt")
set(BASE "src/experiments/language/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "language")

############################################################################
## Target (ga evolution)
############################################################################
set(LG_SRC
    "scenario.cpp"
    "scenario.h"
    "indevaluator.cpp"
    "indevaluator.h"
)
PREPEND(LG_SRC "${BASE}")
add_library(LG_OBJS OBJECT ${LG_SRC})
add_executable(
  lg-evolver
  $<TARGET_OBJECTS:SIMU_OBJS>
  $<TARGET_OBJECTS:LG_OBJS>
  "${BASE}/evolver.cpp")
target_link_libraries(lg-evolver ${CORE_LIBS} pthread)

############################################################################
## Target (evaluation)
############################################################################
add_executable(
  lg-evaluator
  $<TARGET_OBJECTS:SIMU_OBJS>
  $<TARGET_OBJECTS:LG_OBJS>
  "${BASE}/evaluator.cpp")
target_link_libraries(lg-evaluator ${CORE_LIBS})

if (NOT CLUSTER_BUILD)
    ############################################################################
    ### Target (visualizer)
    ############################################################################

    add_executable(
        lg-visualizer
        $<TARGET_OBJECTS:SIMU_OBJS>
        $<TARGET_OBJECTS:VISU_OBJS>
        $<TARGET_OBJECTS:LG_OBJS>
        "${BASE}/visualizer.cpp"
    )
    set_property(TARGET lg-visualizer PROPERTY AUTOMOC ON)
    target_link_libraries(lg-visualizer ${CORE_LIBS} ${GUI_LIBS})
endif()
