# Prints FOHScope's version (the single source: project(... VERSION x.y.z) in CMakeLists.txt).
# Usage: cmake -P scripts/version.cmake
file(READ "${CMAKE_CURRENT_LIST_DIR}/../CMakeLists.txt" _cml)
string(REGEX MATCH "project\\(FOHScope VERSION ([0-9]+\\.[0-9]+\\.[0-9]+)" _m "${_cml}")
if(NOT CMAKE_MATCH_1)
  message(FATAL_ERROR "no project(FOHScope VERSION x.y.z) in CMakeLists.txt")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${CMAKE_MATCH_1}")
