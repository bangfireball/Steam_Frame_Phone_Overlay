#include "phonecast/platform/steamframe/PulseAudioOutput.h"
#include <chrono>
#include <iostream>
#include <thread>

// Optional integration fixture: a private PulseAudio server with module-null-sink.
// Never targets a physical sink and never requires SteamVR.
int main(int argc, char** argv) {
    const bool expectUnavailable = argc == 2 && std::string(argv[1]) == "--unavailable";
    phonecast::platform::steamframe::PulseAudioOutput output;
    std::string error;
    const auto start = std::chrono::steady_clock::now();
    const bool opened = output.Open("",error);
    if (expectUnavailable) {
        if (opened || error.empty() || std::chrono::steady_clock::now() - start > std::chrono::seconds(3)) return 1;
        std::cout << "Unavailable Pulse server fails within the bounded timeout.\n"; return 0;
    }
    if (!opened) { std::cerr << error << '\n'; return 1; }
    std::vector<std::uint8_t> silence(phonecast::core::audio::BytesPerBlock,0);
    unsigned submitted=0;
    const auto deadline = start + std::chrono::seconds(3);
    while (submitted < 30 && std::chrono::steady_clock::now() < deadline) {
        if (output.Write(silence,error)) ++submitted;
        if (!error.empty()) { std::cerr << error << '\n'; return 1; }
        output.LatencyMicros();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    if (!output.Flush(error)) { std::cerr << error << '\n'; return 1; }
    if (!output.Write(silence,error) || !error.empty()) {
        std::cerr << "Null sink write after flush failed: " << error << '\n'; return 1;
    }
    output.Close();
    if (submitted != 30) { std::cerr << "Null sink PCM streaming stalled\n"; return 1; }
    if (output.Open("phonecast-sink-does-not-exist",error)) return 1;
    if (!output.Open("",error)) { std::cerr << error << '\n'; return 1; }
    output.Close();
    std::cout << "Pulse null-sink playback, bounded buffering, invalid sink, and reopen passed.\n";
    return 0;
}
