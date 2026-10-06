#pragma once
#include "phonecast/core/audio/AudioPlayback.h"
#include <memory>
namespace phonecast::platform::steamframe {
class PulseAudioOutput final : public core::audio::IAudioOutput {
public:
    PulseAudioOutput();
    ~PulseAudioOutput() override;
    bool Open(const std::string& device, std::string& error) override;
    bool Write(const std::vector<std::uint8_t>& samples, std::string& error) override;
    std::uint64_t LatencyMicros() override;
    std::string Description() const override;
    void Close() noexcept override;
private:
    struct Implementation;
    std::unique_ptr<Implementation> impl_;
};
}
