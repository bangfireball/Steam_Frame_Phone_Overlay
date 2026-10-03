add_library(phonecast-receiver-lib src/Receiver.cpp)
target_include_directories(phonecast-receiver-lib PUBLIC include)
target_compile_features(phonecast-receiver-lib PUBLIC cxx_std_17)
target_link_libraries(phonecast-receiver-lib PUBLIC phonecast-core phonecast-vr)
phonecast_enable_warnings(phonecast-receiver-lib)

add_executable(phonecast-receiver src/main.cpp)
target_compile_features(phonecast-receiver PRIVATE cxx_std_17)
target_link_libraries(phonecast-receiver PRIVATE phonecast-receiver-lib phonecast-openvr-backend)
phonecast_enable_warnings(phonecast-receiver)

if(MINGW)
    target_link_options(phonecast-receiver PRIVATE -static -static-libgcc -static-libstdc++)
endif()

set_target_properties(phonecast-receiver PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(WIN32)
    add_custom_command(TARGET phonecast-receiver POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:phonecast_openvr>" "$<TARGET_FILE_DIR:phonecast-receiver>"
        COMMENT "Copying the OpenVR runtime client library"
    )
    install(FILES "$<TARGET_FILE:phonecast_openvr>" DESTINATION .)
endif()

add_custom_command(TARGET phonecast-receiver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/platform/openvr/manifest/phonecast-receiver.vrmanifest"
        "$<TARGET_FILE_DIR:phonecast-receiver>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/platform/openvr/input/phonecast-actions.json"
        "$<TARGET_FILE_DIR:phonecast-receiver>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/platform/openvr/input/phonecast-bindings-frame-controller.json"
        "$<TARGET_FILE_DIR:phonecast-receiver>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/platform/openvr/input/phonecast-bindings-generic.json"
        "$<TARGET_FILE_DIR:phonecast-receiver>"
    COMMENT "Copying the receiver application and input manifests"
)

install(TARGETS phonecast-receiver RUNTIME DESTINATION .)
install(FILES
    "${PROJECT_SOURCE_DIR}/platform/openvr/manifest/phonecast-receiver.vrmanifest"
    "${PROJECT_SOURCE_DIR}/platform/openvr/input/phonecast-actions.json"
    "${PROJECT_SOURCE_DIR}/platform/openvr/input/phonecast-bindings-frame-controller.json"
    "${PROJECT_SOURCE_DIR}/platform/openvr/input/phonecast-bindings-generic.json"
    DESTINATION .
)

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME receiver-cli-help COMMAND phonecast-receiver --help)
    add_test(NAME receiver-cli-rejects-invalid-option COMMAND phonecast-receiver --invalid)
    set_tests_properties(receiver-cli-rejects-invalid-option PROPERTIES WILL_FAIL TRUE)
endif()
