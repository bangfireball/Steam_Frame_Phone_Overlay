add_library(phonecast-core
    src/AppConfig.cpp
    src/AudioProtocol.cpp
    src/AudioPlayback.cpp
    src/ConsoleLogger.cpp
    src/DecoderRecoveryController.cpp
    src/GeneratedVideoSource.cpp
    src/StreamProtocol.cpp
)
target_include_directories(phonecast-core PUBLIC include)
target_compile_features(phonecast-core PUBLIC cxx_std_17)
phonecast_enable_warnings(phonecast-core)
