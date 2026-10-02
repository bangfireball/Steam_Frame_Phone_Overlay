add_executable(hello-frame src/main.cpp)
target_compile_features(hello-frame PRIVATE cxx_std_17)
target_link_libraries(hello-frame PRIVATE phonecast_openvr)
phonecast_enable_warnings(hello-frame)

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
