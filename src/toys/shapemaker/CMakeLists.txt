set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "toys")
set(SHAPEMAKER_SRC
    "watchmaker_classes.cpp"
    "main.cpp"
)
PREPEND(SHAPEMAKER_SRC "src/toys/shapemaker")

add_executable(
  shapemaker
  $<TARGET_OBJECTS:SIMU_OBJS>
  $<TARGET_OBJECTS:VISU_OBJS>
  ${SHAPEMAKER_SRC}
)
set_property(TARGET shapemaker PROPERTY AUTOMOC ON)
target_link_libraries(shapemaker ${CORE_LIBS} ${GUI_LIBS})
