cmake_minimum_required(VERSION 3.15)
project(joint_state_xrce_receiver LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MICRO_XRCE_ROOT "" CACHE PATH "Root path containing microxrcedds_client/microcdr headers and libs")

set(_micro_xrce_candidate_roots "")
if(MICRO_XRCE_ROOT)
  list(APPEND _micro_xrce_candidate_roots "${MICRO_XRCE_ROOT}")
endif()

if(MINGW)
  list(APPEND _micro_xrce_candidate_roots
    "C:/msys64/usr/local"
    "/usr/local"
    "C:/msys64/ucrt64"
    "/ucrt64"
  )
else()
  list(APPEND _micro_xrce_candidate_roots "/usr/local")
endif()

set(_micro_xrce_root_resolved "")
foreach(_root IN LISTS _micro_xrce_candidate_roots)
  if(
    EXISTS "${_root}/include/uxr/client/client.h" AND
    EXISTS "${_root}/include/ucdr/microcdr.h" AND
    (
      EXISTS "${_root}/lib/libmicroxrcedds_client.a" OR
      EXISTS "${_root}/lib/libmicroxrcedds_client.dll.a"
    ) AND
    (
      EXISTS "${_root}/lib/libmicrocdr.a" OR
      EXISTS "${_root}/lib/libmicrocdr.dll.a"
    )
  )
    set(_micro_xrce_root_resolved "${_root}")
    break()
  endif()
endforeach()

if(NOT _micro_xrce_root_resolved)
  message(FATAL_ERROR
    "Could not find a valid Micro XRCE dependency root. Checked: ${_micro_xrce_candidate_roots}. "
    "Expected include/uxr/client/client.h, include/ucdr/microcdr.h and libmicroxrcedds_client/libmicrocdr in lib/."
  )
endif()

if(MICRO_XRCE_ROOT AND NOT "${MICRO_XRCE_ROOT}" STREQUAL "${_micro_xrce_root_resolved}")
  message(WARNING
    "MICRO_XRCE_ROOT='${MICRO_XRCE_ROOT}' is not usable in this environment; "
    "using '${_micro_xrce_root_resolved}' instead."
  )
endif()

set(MICRO_XRCE_ROOT "${_micro_xrce_root_resolved}" CACHE PATH "Resolved Micro XRCE root" FORCE)
set(MICRO_XRCE_INCLUDE_DIR "${MICRO_XRCE_ROOT}/include")
set(MICRO_XRCE_LIB_DIR "${MICRO_XRCE_ROOT}/lib")

add_library(joint_state_xrce_receiver SHARED
  src/joint_state_xrce_receiver.cpp
)

add_executable(joint_state_receiver_demo
  src/demo_receiver_main.cpp
)

target_link_libraries(joint_state_receiver_demo
  joint_state_xrce_receiver
)

target_compile_definitions(joint_state_xrce_receiver PRIVATE JOINT_STATE_XRCE_RECEIVER_EXPORTS)

target_include_directories(joint_state_xrce_receiver
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${MICRO_XRCE_INCLUDE_DIR}
)

target_link_directories(joint_state_xrce_receiver PRIVATE ${MICRO_XRCE_LIB_DIR})

target_link_libraries(joint_state_xrce_receiver
  PRIVATE
  microxrcedds_client
  microcdr
)

if(WIN32)
  target_link_libraries(joint_state_xrce_receiver PRIVATE ws2_32 iphlpapi)
endif()
