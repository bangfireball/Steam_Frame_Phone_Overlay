cmake_minimum_required(VERSION 3.24)

project(phonecast-vr VERSION 0.9.0 LANGUAGES CXX)
execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE PHONECAST_BUILD_ID OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git status --porcelain WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE PHONECAST_BUILD_CHANGES ERROR_QUIET)
if(NOT PHONECAST_BUILD_ID)
    set(PHONECAST_BUILD_ID "source")
elseif(PHONECAST_BUILD_CHANGES)
    string(APPEND PHONECAST_BUILD_ID "-dirty")
endif()
add_compile_definitions(PHONECAST_VERSION="${PROJECT_VERSION}" PHONECAST_BUILD_ID="${PHONECAST_BUILD_ID}")

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
    add_subdirectory(platform/support)
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
