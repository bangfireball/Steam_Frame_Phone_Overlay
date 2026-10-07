cmake_minimum_required(VERSION 3.24)

project(phonecast-vr VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

option(PHONECAST_BUILD_RECEIVER "Build the PhoneCast receiver" ON)

function(phonecast_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

include(cmake/OpenVR.cmake)

add_subdirectory(core)
add_subdirectory(vr)

if(PHONECAST_BUILD_RECEIVER)
    add_subdirectory(platform/network)
    add_subdirectory(platform/openvr)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_subdirectory(platform/steam-frame-arm64)
    endif()
    add_subdirectory(apps/receiver)
    add_subdirectory(apps/stream-receiver)
endif()

if(BUILD_TESTING AND PHONECAST_BUILD_RECEIVER)
    add_subdirectory(tests)
endif()
