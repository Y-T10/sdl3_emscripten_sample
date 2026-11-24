cmake_minimum_required(VERSION 4.2.3)
project(Sample
    VERSION 0.0.1
    LANGUAGES CXX
)

add_subdirectory("${CMAKE_SOURCE_DIR}/SDL" EXCLUDE_FROM_ALL)

add_executable(Sample)
set_target_properties(Sample PROPERTIES SUFFIX ".html")
set_target_properties(Sample PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY
  "${CMAKE_CURRENT_SOURCE_DIR}/public"
)

target_sources(Sample PRIVATE main.cpp)
target_link_libraries(Sample PRIVATE SDL3::SDL3)
set_target_properties(Sample PROPERTIES
  LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shell_template.html"
)
add_custom_target(
  LinkFiles ALL
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/Sample.css"
    "$<TARGET_FILE_DIR:Sample>"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Sample.css"
  VERBATIM
)
add_dependencies(Sample LinkFiles)

target_compile_options(Sample PRIVATE -Wall -O2)
target_compile_features(Sample PRIVATE cxx_std_23)
target_link_options(Sample PRIVATE
  --shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell_template.html
  -sEXPORT_ES6 -sMODULARIZE -sEXPORT_NAME="Sample"
  -sENVIRONMENT=['web']
  -flto
  -sEXIT_RUNTIME=1
)

add_custom_command(
  TARGET Sample POST_BUILD
  COMMAND 
    ${CMAKE_COMMAND} -E rename
    "${CMAKE_CURRENT_SOURCE_DIR}/public/Sample.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/public/index.html"
)

## セッターを持たないモジュールへの代入対策
target_link_options(Sample PRIVATE
  -sEXPORTED_RUNTIME_METHODS=requestFullscreen
)
