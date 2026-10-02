if(NOT WIN32)
    return()
endif()

add_library(phonecast-windows-streaming
    ${PROJECT_SOURCE_DIR}/platform/windows-x64/src/DesktopPreview.cpp
    ${PROJECT_SOURCE_DIR}/platform/windows-x64/src/MfH264Decoder.cpp
)
target_include_directories(phonecast-windows-streaming PUBLIC
    ${PROJECT_SOURCE_DIR}/platform/windows-x64/include
)
target_compile_features(phonecast-windows-streaming PUBLIC cxx_std_17)
target_link_libraries(phonecast-windows-streaming PUBLIC phonecast-core mfplat mfuuid ole32)
phonecast_enable_warnings(phonecast-windows-streaming)

add_executable(phonecast-stream-receiver src/main.cpp)
target_compile_features(phonecast-stream-receiver PRIVATE cxx_std_17)
target_link_libraries(phonecast-stream-receiver PRIVATE
    phonecast-core phonecast-windows-streaming ws2_32
)
phonecast_enable_warnings(phonecast-stream-receiver)
set_target_properties(phonecast-stream-receiver PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MINGW)
    target_link_options(phonecast-stream-receiver PRIVATE -static -static-libgcc -static-libstdc++)
endif()

install(TARGETS phonecast-stream-receiver RUNTIME DESTINATION .)

if(BUILD_TESTING)
    add_test(NAME stream-receiver-cli-help COMMAND phonecast-stream-receiver --help)
    add_test(NAME stream-receiver-requires-pair-code COMMAND phonecast-stream-receiver)
    set_tests_properties(stream-receiver-requires-pair-code PROPERTIES WILL_FAIL TRUE)
endif()
