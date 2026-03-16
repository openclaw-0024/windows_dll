cmake_minimum_required(VERSION 3.15)
project(joint_state_xrce_receiver LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MICRO_XRCE_ROOT "" CACHE PATH "Root path containing microxrcedds_client/microcdr headers and libs")

if(NOT MICRO_XRCE_ROOT)
  if(MINGW)
    set(MICRO_XRCE_ROOT "C:/msys64/ucrt64")
  else()
    set(MICRO_XRCE_ROOT "/usr/local")
  endif()
endif()

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
