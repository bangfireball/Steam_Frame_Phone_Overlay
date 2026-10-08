add_library(phonecast-support src/SupportDiagnostics.cpp)
target_include_directories(phonecast-support PUBLIC include)
target_compile_features(phonecast-support PUBLIC cxx_std_17)
phonecast_enable_warnings(phonecast-support)
