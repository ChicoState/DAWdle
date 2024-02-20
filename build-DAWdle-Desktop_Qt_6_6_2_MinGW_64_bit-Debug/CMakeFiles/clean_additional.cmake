# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\DAWdle_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\DAWdle_autogen.dir\\ParseCache.txt"
  "DAWdle_autogen"
  )
endif()
