add_library(phonecast-vr
    src/GlanceController.cpp
    src/OverlayController.cpp
    src/OverlaySettingsStore.cpp
    src/SettingsMenuController.cpp
)
target_include_directories(phonecast-vr PUBLIC include)
target_link_libraries(phonecast-vr PUBLIC phonecast-core)
target_compile_features(phonecast-vr PUBLIC cxx_std_17)
phonecast_enable_warnings(phonecast-vr)
