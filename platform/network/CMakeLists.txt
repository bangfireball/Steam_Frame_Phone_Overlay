add_library(phonecast-network
    src/TcpVideoServer.cpp
    src/LanDiscovery.cpp
)
target_include_directories(phonecast-network PUBLIC include)
target_compile_features(phonecast-network PUBLIC cxx_std_17)
target_link_libraries(phonecast-network PUBLIC phonecast-core)
if(WIN32)
    target_link_libraries(phonecast-network PRIVATE ws2_32)
endif()
phonecast_enable_warnings(phonecast-network)
