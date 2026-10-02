add_library(phonecast-vr INTERFACE)
target_include_directories(phonecast-vr INTERFACE include)
target_link_libraries(phonecast-vr INTERFACE phonecast-core)
