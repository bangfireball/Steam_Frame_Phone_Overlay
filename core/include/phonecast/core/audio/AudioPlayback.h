#pragma once

#include "phonecast/core/audio/AudioProtocol.h"
#include "phonecast/core/streaming/VideoFrame.h"
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <string>

namespace phonecast::core::audio {
// Called only on the output worker. Write is nonblocking: false with no error
// means insufficient space. Close discards queued sound; never drains on quit.
class IAudioOutput {
public:
    virtual ~IAudioOutput() = default;
    virtual bool Open(const std::string& device, std::string& error) = 0;
    virtual bool Write(const std::vector<std::uint8_t>& samples, std::string& error) = 0;
    virtual std::uint64_t LatencyMicros() = 0;
    virtual std::string Description() const = 0;
    virtual void Close() noexcept = 0;
};
struct PlaybackStats {
    std::uint64_t submitted{}, dropped{}, failures{};
    std::size_t queued{};
    std::uint64_t latencyMicros{};
    std::string status{"Audio off"};
};
class AudioPlayback {
public:
    using Factory = std::function<std::unique_ptr<IAudioOutput>()>;
    explicit AudioPlayback(Factory factory);
    ~AudioPlayback();
    void Configure(std::uint64_t epoch);
    void Reset();
    void SetControls(bool muted, float volume, std::string device);
    void Enqueue(Block block);
    PlaybackStats Stats() const;
    std::uint64_t AudibleTimestamp() const;
    void Stop();
private:
    struct Implementation;
    std::unique_ptr<Implementation> impl_;
};

struct VideoPlayoutStats {
    std::uint64_t deadlineReleases{}, overflowReleases{}, coalescedFrames{};
    std::uint64_t lastHoldMicros{};
    std::int64_t estimatedSkewMicros{}; // Picture PTS minus estimated audible PTS.
    bool haveEstimatedSkew{false};
};

// Bounded video coalescing for an active audio clock. Each picture has its own
// 100 ms deadline; releasing one must not discard future pictures or restart
// their deadlines. Eight slots cover that wait at both 30 and 60 FPS.
// Used only on the VR thread, without waiting on a sound device.
class AudioVideoQueue {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr std::size_t MaxPendingFrames = 8;
    void Clear() {
        frames_.clear(); overflowPending_ = false;
        stats_.lastHoldMicros = 0; stats_.estimatedSkewMicros = 0;
        stats_.haveEstimatedSkew = false;
    }
    void Add(VideoFrame frame, Clock::time_point now);
    bool Take(std::uint64_t audibleMicros, Clock::time_point now, VideoFrame& frame);
    std::size_t Size() const { return frames_.size(); }
    VideoPlayoutStats Stats() const { return stats_; }
private:
    struct Entry { VideoFrame frame; Clock::time_point arrival; };
    void RecordPresentation(const Entry& entry, std::uint64_t audibleMicros, Clock::time_point now);
    std::deque<Entry> frames_;
    bool overflowPending_{false};
    VideoPlayoutStats stats_; // Release/coalescing counters persist across Clear.
};
} // namespace phonecast::core::audio
