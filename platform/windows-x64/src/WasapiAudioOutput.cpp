#include "phonecast/platform/windows/WasapiAudioOutput.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <cstring>
#include <chrono>

namespace phonecast::platform::windows {
namespace {
template<class T> void Release(T*& object) { if (object) { object->Release(); object = nullptr; } }
std::wstring Wide(const std::string& value) {
    const int n = MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
    std::wstring out(static_cast<std::size_t>(n),L'\0');
    if (n) MultiByteToWideChar(CP_UTF8,0,value.data(),static_cast<int>(value.size()),out.data(),n);
    return out;
}
std::string Failure(HRESULT hr) { return "WASAPI HRESULT " + std::to_string(static_cast<unsigned long>(hr)); }
}
struct WasapiAudioOutput::Implementation {
    IMMDeviceEnumerator* enumerator{};
    IMMDevice* endpoint{};
    IAudioClient* client{};
    IAudioRenderClient* render{};
    UINT32 capacity{};
    bool com{}, defaultRoute{};
    std::wstring endpointId;
    std::chrono::steady_clock::time_point lastRouteCheck{};
};
WasapiAudioOutput::WasapiAudioOutput() : impl_(std::make_unique<Implementation>()) {}
WasapiAudioOutput::~WasapiAudioOutput() { Close(); }
bool WasapiAudioOutput::Open(const std::string& device, std::string& error) {
    Close(); auto& s = *impl_;
    HRESULT hr = CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if (FAILED(hr)) { error = Failure(hr); return false; } s.com = true;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),reinterpret_cast<void**>(&s.enumerator));
    s.defaultRoute = device.empty();
    if (SUCCEEDED(hr)) {
        hr = device.empty() ? s.enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&s.endpoint)
                           : s.enumerator->GetDevice(Wide(device).c_str(),&s.endpoint);
    }
    if (SUCCEEDED(hr)) {
        LPWSTR id = nullptr;
        hr = s.endpoint->GetId(&id);
        if (SUCCEEDED(hr)) { s.endpointId = id; CoTaskMemFree(id); }
    }
    if (SUCCEEDED(hr)) hr = s.endpoint->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(&s.client));
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM; format.nChannels = 2; format.nSamplesPerSec = 48000;
    format.wBitsPerSample = 16; format.nBlockAlign = 4; format.nAvgBytesPerSec = 192000;
    // Engine conversion/resampling is per-session; never exclusive or global.
    if (SUCCEEDED(hr)) hr = s.client->Initialize(AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
        200000,0,&format,nullptr);
    if (SUCCEEDED(hr)) hr = s.client->GetBufferSize(&s.capacity);
    if (SUCCEEDED(hr) && s.capacity < core::audio::FramesPerBlock) hr = AUDCLNT_E_BUFFER_SIZE_ERROR;
    if (SUCCEEDED(hr)) hr = s.client->GetService(__uuidof(IAudioRenderClient),reinterpret_cast<void**>(&s.render));
    if (SUCCEEDED(hr)) {
        IAudioSessionControl2* session = nullptr;
        if (SUCCEEDED(s.client->GetService(__uuidof(IAudioSessionControl2),reinterpret_cast<void**>(&session)))) {
            session->SetDisplayName(L"PhoneCast playback audio",nullptr);
            session->SetDuckingPreference(TRUE); // Do not duck game sound for this media stream.
            session->Release();
        }
        hr = s.client->Start();
    }
    if (FAILED(hr)) { error = Failure(hr); Close(); return false; }
    s.lastRouteCheck = std::chrono::steady_clock::now(); error.clear(); return true;
}
bool WasapiAudioOutput::Write(const std::vector<std::uint8_t>& samples, std::string& error) {
    auto& s = *impl_; error.clear();
    if (!s.client || samples.size() != core::audio::BytesPerBlock) { error = "Invalid PCM output block"; return false; }
    if (s.defaultRoute && std::chrono::steady_clock::now() - s.lastRouteCheck > std::chrono::seconds(1)) {
        s.lastRouteCheck = std::chrono::steady_clock::now();
        IMMDevice* endpoint = nullptr; LPWSTR id = nullptr;
        HRESULT hr = s.enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&endpoint);
        if (SUCCEEDED(hr)) hr = endpoint->GetId(&id);
        const bool changed = FAILED(hr) || s.endpointId != (id ? id : L"");
        CoTaskMemFree(id); Release(endpoint);
        if (changed) { error = "Default audio output changed; reopening"; return false; }
    }
    UINT32 padding = 0; HRESULT hr = s.client->GetCurrentPadding(&padding);
    if (FAILED(hr)) { error = Failure(hr); return false; }
    // Keep actual engine backlog within 30 ms even on a large endpoint buffer.
    if (padding > 960 || s.capacity < padding + core::audio::FramesPerBlock) return false;
    BYTE* target = nullptr;
    hr = s.render->GetBuffer(core::audio::FramesPerBlock,&target);
    if (SUCCEEDED(hr)) {
        std::memcpy(target,samples.data(),samples.size());
        hr = s.render->ReleaseBuffer(core::audio::FramesPerBlock,0);
    }
    if (FAILED(hr)) { error = Failure(hr); return false; }
    return true;
}
bool WasapiAudioOutput::Flush(std::string& error) {
    auto& s = *impl_; error.clear();
    if (!s.client) { error = "Audio output is closed"; return false; }
    // Reset requires a stopped stream and discards padding without changing
    // the shared endpoint or other applications' sessions.
    HRESULT hr = s.client->Stop();
    if (SUCCEEDED(hr)) hr = s.client->Reset();
    if (SUCCEEDED(hr)) hr = s.client->Start();
    if (FAILED(hr)) { error = Failure(hr); return false; }
    return true;
}
std::uint64_t WasapiAudioOutput::LatencyMicros() {
    UINT32 padding = 0;
    if (!impl_->client || FAILED(impl_->client->GetCurrentPadding(&padding))) return 0;
    REFERENCE_TIME latency = 0;
    impl_->client->GetStreamLatency(&latency);
    return static_cast<std::uint64_t>(padding) * 1000000 / 48000 +
        (latency > 0 ? static_cast<std::uint64_t>(latency / 10) : 0);
}
std::string WasapiAudioOutput::Description() const {
    return impl_->defaultRoute ? "Playing: WASAPI system default" : "Playing: WASAPI selected VR/device output";
}
void WasapiAudioOutput::Close() noexcept {
    auto& s = *impl_;
    if (s.client) { s.client->Stop(); s.client->Reset(); }
    Release(s.render); Release(s.client); Release(s.endpoint); Release(s.enumerator);
    if (s.com) { CoUninitialize(); s.com = false; }
}
}
