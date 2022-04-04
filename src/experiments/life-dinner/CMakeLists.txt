message("  ###############################################################")
message("  Processing Life-dinner CMakeLists.txt")
set(BASE "src/experiments/life-dinner/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "life-dinner")

############################################################################
## Target (ga evolution)
############################################################################
set(LD_SRC
    "scenario.cpp"
    "scenario.h"
    "indevaluator.cpp"
    "indevaluator.h"
)
PREPEND(LD_SRC "${BASE}")
add_library(LD_OBJS OBJECT ${LD_SRC})
add_executable(
  ld-evolver
  $<TARGET_OBJECTS:SIMU_OBJS>
  $<TARGET_OBJECTS:LD_OBJS>
  "${BASE}/evolver.cpp")
target_link_libraries(ld-evolver ${CORE_LIBS} pthread)

############################################################################
## Target (evaluation)
############################################################################
add_executable(
  ld-evaluator
  $<TARGET_OBJECTS:SIMU_OBJS>
  $<TARGET_OBJECTS:LD_OBJS>
  "${BASE}/evaluator.cpp")
target_link_libraries(ld-evaluator ${CORE_LIBS})

############################################################################
### Targets (visualizer)
############################################################################
if (NOT CLUSTER_BUILD)
    add_executable(
        ld-visualizer
        $<TARGET_OBJECTS:SIMU_OBJS>
        $<TARGET_OBJECTS:VISU_OBJS>
        $<TARGET_OBJECTS:LD_OBJS>
        "${BASE}/visualizer.cpp"
    )
    set_property(TARGET ld-visualizer PROPERTY AUTOMOC ON)
    target_link_libraries(ld-visualizer ${CORE_LIBS} ${GUI_LIBS})
endif()
