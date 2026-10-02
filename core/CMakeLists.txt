add_library(phonecast-core
    src/AppConfig.cpp
    src/ConsoleLogger.cpp
    src/GeneratedVideoSource.cpp
)
target_include_directories(phonecast-core PUBLIC include)
target_compile_features(phonecast-core PUBLIC cxx_std_17)
phonecast_enable_warnings(phonecast-core)
