message("  ###############################################################")
message("  Processing Ecosystem CMakeLists.txt")
set(BASE "src/experiments/ecosystem/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "ecosystem")

############################################################################
## Target (simulation)
############################################################################
add_executable(
  es-simulator
  $<TARGET_OBJECTS:SIMU_OBJS>
  "${BASE}/simulator.cpp")
target_link_libraries(es-simulator ${CORE_LIBS})

############################################################################
## Target (evaluation)
############################################################################
add_executable(
  es-evaluator
  $<TARGET_OBJECTS:SIMU_OBJS>
  "${BASE}/evaluator.cpp")
target_link_libraries(es-evaluator ${CORE_LIBS})

############################################################################
## Target (timelines explorer)
############################################################################
find_package(OpenMP)
if (OPENMP_FOUND)
    message("  > OpenMP Found.")
    message("    > OpenMP flags are " ${OpenMP_CXX_LIBRARIES})
  add_executable(
    es-timelines
    $<TARGET_OBJECTS:SIMU_OBJS>
    "${BASE}/timelines.cpp")
  target_link_libraries(es-timelines ${CORE_LIBS} OpenMP::OpenMP_CXX)
endif()


################################################################################
## Target (tester may be with gui)
################################################################################
#if (CLUSTER_BUILD)
#    add_executable(
#        es-tester
#        $<TARGET_OBJECTS:SIMU_OBJS>
#        "src/tests/tester.cpp")
#    target_link_libraries(tester ${CORE_LIBS})
#else()
#    add_executable(
#        es-tester
#        $<TARGET_OBJECTS:SIMU_OBJS>
#        $<TARGET_OBJECTS:VISU_OBJS>
#        "src/tests/tester.cpp")
#    target_link_libraries(tester ${CORE_LIBS} ${GUI_LIBS})
#endif()
