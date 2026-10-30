# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\MusiKCpp_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\MusiKCpp_autogen.dir\\ParseCache.txt"
  "MusiKCpp_autogen"
  )
endif()
