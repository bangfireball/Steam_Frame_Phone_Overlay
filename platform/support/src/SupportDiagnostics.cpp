#include "phonecast/platform/support/SupportDiagnostics.h"
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace phonecast::platform::support {
namespace {
std::string Environment(const char* name) { const char* value = std::getenv(name); return value ? value : ""; }
}
std::string SafeDiagnosticLine(const std::string& raw) {
    std::string line = raw;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.size() > 8192) return {};
    if (line.rfind("[diagnostics] ", 0) == 0) {
        std::istringstream tokens(line.substr(14));
        std::string token;
        while (tokens >> token) {
            const auto equals = token.find('=');
            if (equals == std::string::npos || equals == 0 || equals + 1 == token.size()) return {};
            const auto key = token.substr(0, equals), value = token.substr(equals + 1);
            if (key.find_first_not_of("abcdefghijklmnopqrstuvwxyz-") != std::string::npos) return {};
            // Only the existing numeric counters plus the fixed connected flag.
            if (key == "connected") { if (value != "yes" && value != "no") return {}; }
            else {
                bool known = false;
                for (const auto allowed : {"rx-fps", "decode-fps", "render-fps", "bitrate-mbps", "decode-ms", "render-ms",
                    "queue-ms", "queue-max-ms", "queue-depth", "keyframes", "dropped", "resyncs",
                    "decoder-recovery-triggers", "decoder-recovery-successes", "decoder-recovery-failed-attempts",
                    "audio-submitted", "audio-dropped", "audio-failures", "audio-sequence-gaps", "audio-timestamp-gaps",
                    "audio-reanchors", "audio-flushes", "audio-opens", "audio-stale-drops", "audio-overflow-drops",
                    "audio-rejected-drops", "audio-max-open-ms", "audio-queued", "audio-output-latency-ms", "audio-bitrate-mbps",
                    "video-sync-queued", "video-sync-deadline-releases", "video-sync-overflow-releases", "video-sync-coalesced",
                    "video-sync-last-hold-ms", "video-sync-skew-valid", "video-sync-estimated-skew-ms",
                    "dispatch-max-messages", "dispatch-max-ms", "presentation-max-gap-ms", "loop-max-ms",
                    "process-cpu-percent", "working-set-mb", "vr-total-gpu-ms", "vr-compositor-gpu-ms", "vr-compositor-cpu-ms",
                    "vr-dropped", "vr-mispresented"}) if (key == allowed) known = true;
                if (!known || value.find_first_not_of("0123456789.eE+-") != std::string::npos) return {};
            }
        }
        return line;
    }
    const std::array<const char*, 7> network{
        "Rejected a client with an invalid pairing code", "Phone connected and authenticated",
        "TCP client connected; waiting for pairing handshake", "Phone disconnected; waiting for reconnection",
        "Could not create listening socket", "Rejected malformed remote-control gate status", "Phone connected; remote control ready"};
    for (const auto message : network) if (line == std::string("[network] ") + message) return line;
    for (const auto prefix : {"[network] Waiting for phone on TCP port ", "[network] Could not listen on TCP port "}) {
        const std::string start(prefix);
        if (line.rfind(start, 0) == 0 && line.substr(start.size()).find_first_not_of("0123456789") == std::string::npos) return line;
    }
    return {};
}
bool ExportDiagnostics(std::string& filename, std::string& error) {
    try {
#ifdef _WIN32
        const auto home = Environment("USERPROFILE");
        const auto state = std::filesystem::path(Environment("LOCALAPPDATA")) / "PhoneCastVR";
#else
        const auto home = Environment("HOME");
        const auto configuredState = Environment("XDG_STATE_HOME");
        const auto state = (configuredState.empty() ? std::filesystem::path(home) / ".local/state" : std::filesystem::path(configuredState)) / "phonecast-vr";
#endif
        if (home.empty()) { error = "HOME UNAVAILABLE"; return false; }
        auto downloads = std::filesystem::path(home) / "Downloads";
#ifndef _WIN32
        const auto configuredConfig = Environment("XDG_CONFIG_HOME");
        const auto config = configuredConfig.empty() ? std::filesystem::path(home) / ".config" : std::filesystem::path(configuredConfig);
        std::ifstream directories(config / "user-dirs.dirs");
        std::string line;
        while (std::getline(directories, line)) {
            const std::string prefix = "XDG_DOWNLOAD_DIR=\"";
            if (line.rfind(prefix, 0) != 0 || line.back() != '"') continue;
            auto value = line.substr(prefix.size(), line.size() - prefix.size() - 1);
            if (value.rfind("$HOME/", 0) == 0) value = home + value.substr(5);
            if (value.find('$') == std::string::npos && std::filesystem::path(value).is_absolute()) downloads = value;
        }
#endif
        std::filesystem::create_directories(downloads);
        std::string report = std::string("PhoneCast ") + PHONECAST_VERSION + "\nBuild " + PHONECAST_BUILD_ID +
            "\nBounded safe diagnostics; arbitrary messages/private payloads omitted.\n";
        std::ifstream log(state / "receiver.log", std::ios::binary);
        if (log) {
            log.seekg(0, std::ios::end);
            const auto length = log.tellg();
            const auto start = length > std::streamoff(262144) ? length - std::streamoff(262144) : std::streampos(0);
            log.seekg(start);
            std::string data(262144, '\0');
            log.read(data.data(), static_cast<std::streamsize>(data.size()));
            data.resize(static_cast<std::size_t>(log.gcount()));
            std::istringstream lines(data); std::string line;
            if (start > std::streampos(0)) std::getline(lines, line);
            std::string safe;
            while (std::getline(lines, line)) {
                const auto filtered = SafeDiagnosticLine(line);
                if (filtered.empty()) continue;
                safe += filtered + '\n';
                while (safe.size() > 65536) safe.erase(0, safe.find('\n') + 1);
            }
            report += safe.empty() ? "No eligible log events.\n" : safe;
        } else report += "Receiver log unavailable at the standard state location.\n";
        const auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const auto outputPath = downloads / ("phonecast-headset-" + std::to_string(stamp) + ".txt");
        std::ofstream output(outputPath, std::ios::binary);
        if (!output) { error = "DOWNLOADS NOT WRITABLE"; return false; }
        output << report; output.close();
        if (!output) { std::filesystem::remove(outputPath); error = "EXPORT WRITE FAILED"; return false; }
#ifndef _WIN32
        std::filesystem::permissions(outputPath, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
#endif
        filename = outputPath.filename().string(); error.clear(); return true;
    } catch (const std::exception&) { error = "EXPORT FAILED: CHECK STORAGE"; return false; }
}
}
