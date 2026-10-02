include(FetchContent)

# Pinned OpenVR SDK 2.15.6. FetchContent obtains both the headers and the
# official Windows library. Linux builds compile openvr_api from source so the
# same configuration can target ARM64.
FetchContent_Declare(
    openvr
    GIT_REPOSITORY https://github.com/ValveSoftware/openvr.git
    GIT_TAG 0924064316de3effbcd1acf1e309182a2deb1c05
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _phonecast_no_automatic_add_subdirectory
)
FetchContent_MakeAvailable(openvr)

if(WIN32)
    add_library(phonecast_openvr SHARED IMPORTED GLOBAL)
    set_target_properties(phonecast_openvr PROPERTIES
        IMPORTED_IMPLIB "${openvr_SOURCE_DIR}/lib/win64/openvr_api.lib"
        IMPORTED_LOCATION "${openvr_SOURCE_DIR}/bin/win64/openvr_api.dll"
        INTERFACE_INCLUDE_DIRECTORIES "${openvr_SOURCE_DIR}/headers"
    )
    set(HELLO_FRAME_OPENVR_TARGET phonecast_openvr)
else()
    set(BUILD_SHARED OFF CACHE BOOL "Build OpenVR as a static library" FORCE)
    set(BUILD_FRAMEWORK OFF CACHE BOOL "Do not build an OpenVR framework" FORCE)
    set(USE_LIBCXX OFF CACHE BOOL "Use the platform C++ standard library" FORCE)
    add_subdirectory("${openvr_SOURCE_DIR}" "${openvr_BINARY_DIR}" EXCLUDE_FROM_ALL)
    set(HELLO_FRAME_OPENVR_TARGET openvr_api)
endif()

add_executable(hello-frame src/main.cpp)
target_compile_features(hello-frame PRIVATE cxx_std_17)
target_link_libraries(hello-frame PRIVATE ${HELLO_FRAME_OPENVR_TARGET})

if(MSVC)
    target_compile_options(hello-frame PRIVATE /W4 /permissive-)
else()
    target_compile_options(hello-frame PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(MINGW)
    # Keep the experiment deployable without a separate MinGW runtime bundle.
    target_link_options(hello-frame PRIVATE -static -static-libgcc -static-libstdc++)
endif()

set_target_properties(hello-frame PROPERTIES
    OUTPUT_NAME "hello-frame"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(WIN32)
    add_custom_command(TARGET hello-frame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:phonecast_openvr>"
            "$<TARGET_FILE_DIR:hello-frame>"
        COMMENT "Copying the OpenVR runtime client library"
    )
    install(FILES "$<TARGET_FILE:phonecast_openvr>" DESTINATION .)
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME hello-frame-cli-help COMMAND hello-frame --help)
    add_test(NAME hello-frame-cli-rejects-invalid-argument COMMAND hello-frame --invalid)
    set_tests_properties(hello-frame-cli-rejects-invalid-argument PROPERTIES WILL_FAIL TRUE)
endif()

install(TARGETS hello-frame RUNTIME DESTINATION .)
install(FILES manifest/hello-frame.vrmanifest DESTINATION .)
