add_library(phonecast-openvr-backend src/OpenVrOverlayRenderer.cpp)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(phonecast-openvr-backend PRIVATE src/SteamFrameVulkanTexture.cpp)
    # OpenVR pins a Vulkan 1.0 header for its sample renderer. Using that header
    # plus runtime loading keeps ARM64 cross-builds independent of host Vulkan
    # development packages while the headset supplies libvulkan.so.1.
    target_include_directories(phonecast-openvr-backend PRIVATE
        "${openvr_SOURCE_DIR}/samples/thirdparty/vulkan-1.0.49.0/include"
    )
endif()
target_include_directories(phonecast-openvr-backend PUBLIC include)
target_compile_features(phonecast-openvr-backend PUBLIC cxx_std_17)
target_link_libraries(phonecast-openvr-backend PUBLIC phonecast-vr PRIVATE phonecast_openvr)
if(WIN32)
    target_link_libraries(phonecast-openvr-backend PRIVATE d3d11 dxgi)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(phonecast-openvr-backend PRIVATE ${CMAKE_DL_LIBS})
endif()
phonecast_enable_warnings(phonecast-openvr-backend)
