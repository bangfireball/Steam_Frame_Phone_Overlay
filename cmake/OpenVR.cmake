include_guard(GLOBAL)
include(FetchContent)

# OpenVR SDK 2.15.6. Windows uses Valve's client DLL; Linux builds the
# client from source so the same backend can target x64 and ARM64.
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
else()
    set(BUILD_SHARED OFF CACHE BOOL "Build OpenVR statically" FORCE)
    set(BUILD_FRAMEWORK OFF CACHE BOOL "Do not build an OpenVR framework" FORCE)
    set(USE_LIBCXX OFF CACHE BOOL "Use the platform C++ library" FORCE)
    add_subdirectory("${openvr_SOURCE_DIR}" "${openvr_BINARY_DIR}" EXCLUDE_FROM_ALL)
    add_library(phonecast_openvr ALIAS openvr_api)
endif()
