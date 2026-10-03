if(NOT WIN32)
    return()
endif()

add_library(phonecast-windows-streaming
    ${PROJECT_SOURCE_DIR}/platform/windows-x64/src/DesktopPreview.cpp
    ${PROJECT_SOURCE_DIR}/platform/windows-x64/src/MfH264Decoder.cpp
    ${PROJECT_SOURCE_DIR}/platform/windows-x64/src/TcpVideoServer.cpp
)
target_include_directories(phonecast-windows-streaming PUBLIC
    ${PROJECT_SOURCE_DIR}/platform/windows-x64/include
)
target_compile_features(phonecast-windows-streaming PUBLIC cxx_std_17)
target_link_libraries(phonecast-windows-streaming PUBLIC phonecast-core mfplat mfuuid ole32 ws2_32)
phonecast_enable_warnings(phonecast-windows-streaming)

add_executable(phonecast-stream-receiver src/main.cpp)
target_compile_features(phonecast-stream-receiver PRIVATE cxx_std_17)
target_link_libraries(phonecast-stream-receiver PRIVATE
    phonecast-core phonecast-windows-streaming
)
phonecast_enable_warnings(phonecast-stream-receiver)
set_target_properties(phonecast-stream-receiver PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(phonecast-vr-stream-receiver src/vr_main.cpp)
target_compile_features(phonecast-vr-stream-receiver PRIVATE cxx_std_17)
target_link_libraries(phonecast-vr-stream-receiver PRIVATE
    phonecast-core phonecast-vr phonecast-windows-streaming phonecast-openvr-backend
)
phonecast_enable_warnings(phonecast-vr-stream-receiver)
set_target_properties(phonecast-vr-stream-receiver PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(MINGW)
    target_link_options(phonecast-stream-receiver PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(phonecast-vr-stream-receiver PRIVATE -static -static-libgcc -static-libstdc++)
endif()

add_custom_command(TARGET phonecast-vr-stream-receiver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:phonecast_openvr>" "$<TARGET_FILE_DIR:phonecast-vr-stream-receiver>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/platform/openvr/manifest/phonecast-receiver.vrmanifest"
        "$<TARGET_FILE_DIR:phonecast-vr-stream-receiver>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/platform/openvr/input/phonecast-actions.json"
        "$<TARGET_FILE_DIR:phonecast-vr-stream-receiver>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/platform/openvr/input/phonecast-bindings-frame-controller.json"
        "$<TARGET_FILE_DIR:phonecast-vr-stream-receiver>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/platform/openvr/input/phonecast-bindings-generic.json"
        "$<TARGET_FILE_DIR:phonecast-vr-stream-receiver>"
    COMMENT "Copying the OpenVR runtime, manifests, and input bindings"
)

install(TARGETS phonecast-stream-receiver phonecast-vr-stream-receiver RUNTIME DESTINATION .)
install(FILES
    "${PROJECT_SOURCE_DIR}/platform/openvr/manifest/phonecast-receiver.vrmanifest"
    "${PROJECT_SOURCE_DIR}/platform/openvr/input/phonecast-actions.json"
    "${PROJECT_SOURCE_DIR}/platform/openvr/input/phonecast-bindings-frame-controller.json"
    "${PROJECT_SOURCE_DIR}/platform/openvr/input/phonecast-bindings-generic.json"
    DESTINATION .
)

if(BUILD_TESTING)
    add_test(NAME stream-receiver-cli-help COMMAND phonecast-stream-receiver --help)
    add_test(NAME vr-stream-receiver-cli-help COMMAND phonecast-vr-stream-receiver --help)
    add_test(NAME stream-receiver-requires-pair-code COMMAND phonecast-stream-receiver)
    set_tests_properties(stream-receiver-requires-pair-code PROPERTIES WILL_FAIL TRUE)
    add_test(NAME vr-diagnostic-cli-help
        COMMAND phonecast-vr-stream-receiver --diagnostic-visible --help)
    set_tests_properties(vr-diagnostic-cli-help PROPERTIES
        PASS_REGULAR_EXPRESSION "diagnostic-visible")
    add_test(NAME vr-diagnostic-requires-pair-code
        COMMAND phonecast-vr-stream-receiver --diagnostic-visible)
    set_tests_properties(vr-diagnostic-requires-pair-code PROPERTIES WILL_FAIL TRUE)
endif()
