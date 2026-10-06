#include "phonecast/platform/steamframe/PulseAudioOutput.h"
#include <pulse/pulseaudio.h>
#include <dlfcn.h>
#include <chrono>
#include <thread>
#include <cstring>

namespace phonecast::platform::steamframe {
struct PulseAudioOutput::Implementation {
    void* library{};
    pa_mainloop* loop{};
    pa_context* context{};
    pa_stream* stream{};
#define PULSE_FUNCTION(name) decltype(&pa_##name) name{}
    PULSE_FUNCTION(mainloop_new); PULSE_FUNCTION(mainloop_free);
    PULSE_FUNCTION(mainloop_get_api); PULSE_FUNCTION(mainloop_iterate);
    PULSE_FUNCTION(context_new); PULSE_FUNCTION(context_connect);
    PULSE_FUNCTION(context_get_state); PULSE_FUNCTION(context_errno);
    PULSE_FUNCTION(context_disconnect); PULSE_FUNCTION(context_unref);
    PULSE_FUNCTION(stream_new); PULSE_FUNCTION(stream_connect_playback);
    PULSE_FUNCTION(stream_get_state); PULSE_FUNCTION(stream_writable_size);
    PULSE_FUNCTION(stream_write); PULSE_FUNCTION(stream_get_latency);
    PULSE_FUNCTION(stream_get_device_name); PULSE_FUNCTION(stream_disconnect);
    PULSE_FUNCTION(stream_unref); PULSE_FUNCTION(strerror);
#undef PULSE_FUNCTION
    bool Load(std::string& error) {
        if (library) return true;
        library = dlopen("libpulse.so.0",RTLD_NOW | RTLD_LOCAL);
        if (!library) { error = "libpulse.so.0 unavailable"; return false; }
#define LOAD(name) { void* symbol = dlsym(library,"pa_" #name); \
    static_assert(sizeof(symbol) == sizeof(name)); std::memcpy(&name,&symbol,sizeof(symbol)); \
    if (!name) { error = "Missing PulseAudio symbol: " #name; dlclose(library); library=nullptr; return false; } }
        LOAD(mainloop_new); LOAD(mainloop_free); LOAD(mainloop_get_api); LOAD(mainloop_iterate);
        LOAD(context_new); LOAD(context_connect); LOAD(context_get_state); LOAD(context_errno);
        LOAD(context_disconnect); LOAD(context_unref); LOAD(stream_new); LOAD(stream_connect_playback);
        LOAD(stream_get_state); LOAD(stream_writable_size); LOAD(stream_write); LOAD(stream_get_latency);
        LOAD(stream_get_device_name); LOAD(stream_disconnect); LOAD(stream_unref); LOAD(strerror);
#undef LOAD
        return true;
    }
    bool Pump() {
        int result = 0;
        // Never block on server I/O or drain in the output worker.
        if (!loop || mainloop_iterate(loop,0,&result) < 0) return false;
        return context && PA_CONTEXT_IS_GOOD(context_get_state(context)) &&
            (!stream || PA_STREAM_IS_GOOD(stream_get_state(stream)));
    }
    std::string Error() const { return context ? strerror(context_errno(context)) : "PulseAudio initialization failed"; }
};
PulseAudioOutput::PulseAudioOutput() : impl_(std::make_unique<Implementation>()) {}
PulseAudioOutput::~PulseAudioOutput() {
    Close(); if (impl_->library) dlclose(impl_->library);
}
bool PulseAudioOutput::Open(const std::string& device, std::string& error) {
    Close(); auto& s = *impl_;
    if (!s.Load(error)) return false;
    s.loop = s.mainloop_new();
    if (s.loop) s.context = s.context_new(s.mainloop_get_api(s.loop),"PhoneCast VR");
    if (!s.context || s.context_connect(s.context,nullptr,PA_CONTEXT_NOAUTOSPAWN,nullptr) < 0) {
        error = s.Error(); Close(); return false;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (s.context_get_state(s.context) != PA_CONTEXT_READY) {
        if (!s.Pump() || std::chrono::steady_clock::now() >= deadline) {
            error = "Pulse server unavailable: " + s.Error(); Close(); return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    const pa_sample_spec format{PA_SAMPLE_S16LE,48000,2};
    s.stream = s.stream_new(s.context,"Phone playback",&format,nullptr);
    const pa_buffer_attr attr{19200,3840,1920,1920,static_cast<std::uint32_t>(-1)};
    const auto flags = static_cast<pa_stream_flags_t>(PA_STREAM_ADJUST_LATENCY |
        PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_INTERPOLATE_TIMING);
    if (!s.stream || s.stream_connect_playback(s.stream,device.empty() ? nullptr : device.c_str(),
            &attr,flags,nullptr,nullptr) < 0) {
        error = s.Error(); Close(); return false;
    }
    while (s.stream_get_state(s.stream) != PA_STREAM_READY) {
        if (!s.Pump() || std::chrono::steady_clock::now() >= deadline) {
            error = "Pulse output unavailable: " + s.Error(); Close(); return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    error.clear(); return true;
}
bool PulseAudioOutput::Write(const std::vector<std::uint8_t>& samples, std::string& error) {
    auto& s = *impl_; error.clear();
    if (!s.Pump() || !s.stream || s.stream_get_state(s.stream) != PA_STREAM_READY) {
        error = s.Error(); return false;
    }
    const auto available = s.stream_writable_size(s.stream);
    if (available == static_cast<std::size_t>(-1)) { error = s.Error(); return false; }
    // Request/observe bounded latency rather than relying on server defaults.
    if (LatencyMicros() > 100000) {
        error = "Pulse sink latency exceeds the 100 ms audio budget"; return false;
    }
    if (available < samples.size()) return false;
    if (s.stream_write(s.stream,samples.data(),samples.size(),nullptr,0,PA_SEEK_RELATIVE) < 0) {
        error = s.Error(); return false;
    }
    return true;
}
std::uint64_t PulseAudioOutput::LatencyMicros() {
    auto& s = *impl_; if (!s.Pump() || !s.stream) return 0;
    pa_usec_t latency = 0; int negative = 0;
    if (s.stream_get_latency(s.stream,&latency,&negative) < 0 || negative) return 0;
    return latency;
}
std::string PulseAudioOutput::Description() const {
    const auto& s = *impl_;
    const char* sink = s.stream ? s.stream_get_device_name(s.stream) : nullptr;
    return std::string("Playing: PulseAudio/PipeWire ") + (sink ? sink : "default sink");
}
void PulseAudioOutput::Close() noexcept {
    auto& s = *impl_;
    if (s.stream) { s.stream_disconnect(s.stream); s.stream_unref(s.stream); s.stream = nullptr; }
    if (s.context) { s.context_disconnect(s.context); s.context_unref(s.context); s.context = nullptr; }
    if (s.loop) { s.mainloop_free(s.loop); s.loop = nullptr; }
}
}
