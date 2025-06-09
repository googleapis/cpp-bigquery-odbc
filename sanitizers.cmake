#
# Copyright (C) 2018 by George Cave - gcave@stablecoder.ca
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

set(USE_SANITIZER
    ""
    CACHE
      STRING
      "Compile with a sanitizer. Only supported option: Address"
)

function(append value)
  foreach(variable ${ARGN})
    set(${variable}
        "${${variable}} ${value}"
        PARENT_SCOPE)
  endforeach()
endfunction()

if(USE_SANITIZER)
  if(NOT USE_SANITIZER MATCHES "^[Aa]ddress$")
    message(FATAL_ERROR "Only 'Address' sanitizer is supported. Provided: ${USE_SANITIZER}")
  endif()

  append("-fno-omit-frame-pointer" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)

  if(UNIX)
    if(uppercase_CMAKE_BUILD_TYPE STREQUAL "DEBUG")
      append("-O1" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)
    endif()
    message(STATUS "Building with Address sanitizer")
    append("-fsanitize=address" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)

  elseif(MSVC)
    message(STATUS "Building with Address sanitizer")
    append("-fsanitize=address" CMAKE_C_FLAGS CMAKE_CXX_FLAGS)

  else()
    message(FATAL_ERROR "USE_SANITIZER is not supported on this platform.")
  endif()
endif()
