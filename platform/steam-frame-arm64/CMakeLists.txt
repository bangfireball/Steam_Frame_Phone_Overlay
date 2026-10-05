if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif()

add_library(phonecast-steam-frame
    src/ProcessPerformanceSampler.cpp
    src/StandaloneRuntime.cpp
    src/V4l2H264Decoder.cpp
)
target_include_directories(phonecast-steam-frame PUBLIC include)
target_compile_features(phonecast-steam-frame PUBLIC cxx_std_17)
target_link_libraries(phonecast-steam-frame PUBLIC phonecast-core)
phonecast_enable_warnings(phonecast-steam-frame)
