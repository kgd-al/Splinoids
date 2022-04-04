message("  ###############################################################")
message("  Processing Life-dinner CMakeLists.txt")

############################################################################
## Target (ga evolution)
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
## Target (evaluation)
############################################################################
add_executable(
  pp-evaluator
  $<TARGET_OBJECTS:SIMU_OBJS>
  $<TARGET_OBJECTS:PP_OBJS>
  "src/alife2021/evaluator.cpp")
target_link_libraries(pp-evaluator ${CORE_LIBS})

############################################################################
### Targets (visualizer)
############################################################################
if (NOT CLUSTER_BUILD)
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
