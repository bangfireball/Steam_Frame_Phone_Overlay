cmake_minimum_required(VERSION 3.24)

project(phonecast-vr VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

option(PHONECAST_BUILD_HELLO_FRAME "Build the Sprint 0 Hello Frame experiment" ON)

if(PHONECAST_BUILD_HELLO_FRAME)
    add_subdirectory(experiments/hello-frame)
endif()
