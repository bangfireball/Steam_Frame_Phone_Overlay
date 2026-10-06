#include "phonecast/core/audio/AudioPlayback.h"
#include "phonecast/core/audio/MediaClock.h"
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace phonecast::core::audio {
namespace {
using Clock = std::chrono::steady_clock;
void Scale(std::vector<std::uint8_t>& samples, float volume) {
    if (volume == 1.0F) return;
    for (std::size_t i = 0; i + 1 < samples.size(); i += 2) {
        const auto bits = static_cast<std::uint16_t>(samples[i] | (samples[i+1] << 8));
        const int value = bits >= 0x8000 ? static_cast<int>(bits) - 65536 : bits;
        const auto result = static_cast<std::uint16_t>(static_cast<int>(std::lround(value * volume)));
        samples[i] = static_cast<std::uint8_t>(result);
        samples[i+1] = static_cast<std::uint8_t>(result >> 8);
    }
}
}
struct AudioPlayback::Implementation {
    explicit Implementation(Factory create) : factory(std::move(create)), worker([this] { Run(); }) {}
    ~Implementation() { Stop(); }
    struct Queued { Block block; Clock::time_point arrival; };
    void Stop() {
        { std::lock_guard<std::mutex> lock(mutex); running = false; queue.clear(); }
        wake.notify_all();
        if (worker.joinable()) worker.join();
    }
    void Run() {
        auto output = factory();
        bool opened = false;
        unsigned attempts = 0;
        std::uint64_t localRevision = 0, localTimeline = 0;
        std::uint64_t endPts = 0;
        Clock::time_point retryAt{}, startAt{}, lastWrite{};
        std::unique_lock<std::mutex> lock(mutex);
        while (running) {
            if (revision != localRevision) {
                localRevision = revision;
                lock.unlock(); output->Close(); lock.lock();
                opened = false; attempts = 0; endPts = 0;
                localTimeline = timeline;
                mediaClock.Reset(); stats.latencyMicros = 0;
                retryAt = {}; startAt = Clock::now() + std::chrono::milliseconds(20);
            }
            if (localTimeline != timeline) {
                localTimeline = timeline;
                const auto flushingRevision = revision;
                mediaClock.Reset(); endPts = 0; stats.latencyMicros = 0;
                if (opened) {
                    lock.unlock(); std::string error; const bool ok = output->Flush(error); lock.lock();
                    if (flushingRevision != revision) continue;
                    if (ok) { ++stats.flushes; }
                    else {
                        ++stats.failures; stats.status = "Audio flush failed: " + error;
                        opened = false;
                        lock.unlock(); output->Close(); lock.lock();
                        retryAt = Clock::now()+std::chrono::seconds(1);
                    }
                }
            }
            if (epoch == 0 || muted) {
                stats.status = muted ? "Phone audio muted" : "Audio off";
                wake.wait_for(lock,std::chrono::milliseconds(10));
                continue;
            }
            const auto now = Clock::now();
            while (!queue.empty() && now - queue.front().arrival > std::chrono::milliseconds(100)) {
                queue.pop_front(); ++stats.dropped; ++stats.staleDrops;
            }
            if (!opened && !queue.empty() && attempts < 3 && now >= retryAt) {
                const auto selected = device;
                const auto openingRevision = revision;
                ++attempts; ++stats.opens;
                const auto openStarted = Clock::now();
                lock.unlock(); std::string error; const bool ok = output->Open(selected,error); lock.lock();
                stats.maximumOpenMicros = std::max(stats.maximumOpenMicros,static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-openStarted).count()));
                if (openingRevision != revision) continue;
                opened = ok;
                if (ok) { stats.status = output->Description(); startAt = Clock::now() + std::chrono::milliseconds(20); }
                else { ++stats.failures; stats.status = "Audio unavailable: " + error; retryAt = Clock::now() + std::chrono::seconds(1); }
            }
            if (opened && now >= startAt && !queue.empty()) {
                auto block = queue.front().block;
                const auto writingRevision = revision;
                const auto writingTimeline = timeline;
                Scale(block.samples,volume);
                lock.unlock(); std::string error; const bool accepted = output->Write(block.samples,error); lock.lock();
                if (writingRevision != revision || writingTimeline != timeline) continue;
                if (!error.empty()) {
                    ++stats.failures; stats.status = "Audio output failed: " + error;
                    opened = false; mediaClock.Reset(); endPts = 0;
                    lock.unlock(); output->Close(); lock.lock();
                    retryAt = Clock::now() + std::chrono::seconds(1);
                } else if (accepted) {
                    if (!queue.empty() && queue.front().block.epoch == block.epoch &&
                        queue.front().block.sequence == block.sequence) queue.pop_front();
                    ++stats.submitted;
                    endPts = block.timestampMicros + 10000;
                    lastWrite = Clock::now();
                }
            }
            if (opened) {
                const auto observingRevision = revision;
                const auto observingTimeline = timeline;
                lock.unlock(); const auto latency = output->LatencyMicros(); lock.lock();
                if (observingRevision != revision || observingTimeline != timeline) continue;
                stats.latencyMicros = latency;
                // No extrapolation past the last submitted sample. If the source
                // stalls, return to video-only presentation rather than freeze it.
                if (endPts > latency && Clock::now() - lastWrite < std::chrono::milliseconds(100))
                    mediaClock.Observe(endPts-latency,endPts,Clock::now());
            }
            wake.wait_for(lock,std::chrono::milliseconds(2));
        }
        lock.unlock(); output->Close();
    }
    Factory factory;
    mutable std::mutex mutex;
    std::condition_variable wake;
    bool running{true}, muted{false}, haveSequence{false};
    float volume{0.7F};
    std::string device;
    std::uint64_t epoch{}, revision{1}, timeline{1}, sequence{}, lastPts{};
    MediaClock mediaClock;
    std::deque<Queued> queue;
    PlaybackStats stats;
    std::thread worker;
};
AudioPlayback::AudioPlayback(Factory factory) : impl_(std::make_unique<Implementation>(std::move(factory))) {}
AudioPlayback::~AudioPlayback() = default;
void AudioPlayback::Configure(std::uint64_t epoch) {
    auto& s = *impl_; std::lock_guard<std::mutex> lock(s.mutex);
    s.epoch = epoch; ++s.revision; s.stats.dropped += s.queue.size(); s.queue.clear();
    s.haveSequence = false; s.lastPts = 0; s.mediaClock.Reset(); s.wake.notify_all();
}
void AudioPlayback::Reset() { Configure(0); }
void AudioPlayback::SetControls(bool muted, float volume, std::string device) {
    auto& s = *impl_; std::lock_guard<std::mutex> lock(s.mutex);
    volume = std::isfinite(volume) ? std::clamp(volume,0.0F,1.0F) : 0.0F;
    if (muted != s.muted || device != s.device) {
        ++s.revision; s.stats.dropped += s.queue.size(); s.queue.clear(); s.mediaClock.Reset();
    }
    s.muted = muted; s.volume = volume; s.device = std::move(device); s.wake.notify_all();
}
void AudioPlayback::Enqueue(Block block) {
    auto& s = *impl_; std::lock_guard<std::mutex> lock(s.mutex);
    if (block.epoch != s.epoch || s.epoch == 0 || block.samples.size() != BytesPerBlock ||
        block.timestampMicros == 0 || s.muted || !s.running ||
        (s.haveSequence && (block.sequence <= s.sequence || block.timestampMicros <= s.lastPts))) {
        ++s.stats.dropped; ++s.stats.rejectedDrops; return;
    }
    if (s.haveSequence) {
        if (block.sequence - s.sequence > 1) ++s.stats.sequenceGaps;
        if (block.timestampMicros - s.lastPts > 20000) ++s.stats.timestampGaps;
    }
    // Missing packets are not device failure. Large forward gaps discard both
    // application and backend sound on the worker, retaining the healthy device.
    if (s.haveSequence && block.timestampMicros - s.lastPts > 100000) {
        s.stats.dropped += s.queue.size(); s.queue.clear(); ++s.timeline; s.mediaClock.Reset();
        ++s.stats.reanchors;
    }
    s.sequence = block.sequence; s.lastPts = block.timestampMicros; s.haveSequence = true;
    if (s.queue.size() >= 10) { s.queue.pop_front(); ++s.stats.dropped; ++s.stats.overflowDrops; }
    s.queue.push_back({std::move(block),Clock::now()}); s.wake.notify_all();
}
PlaybackStats AudioPlayback::Stats() const {
    auto& s = *impl_; std::lock_guard<std::mutex> lock(s.mutex);
    auto stats = s.stats; stats.queued = s.queue.size(); return stats;
}
std::uint64_t AudioPlayback::AudibleTimestamp() const {
    auto& s = *impl_; std::lock_guard<std::mutex> lock(s.mutex);
    return s.mediaClock.Timestamp(Clock::now());
}
void AudioPlayback::Stop() { impl_->Stop(); }
void AudioVideoQueue::Add(VideoFrame frame, Clock::time_point now) {
    if (frames_.size() >= MaxPendingFrames) {
        frames_.pop_front(); ++stats_.coalescedFrames;
        // Even unexpectedly high input rates must not keep evicting a waiting
        // picture and thus renew the deadline forever. Force progress on Take.
        overflowPending_ = true;
    }
    frames_.push_back({std::move(frame),now});
}
void AudioVideoQueue::RecordPresentation(const Entry& entry, std::uint64_t audibleMicros, Clock::time_point now) {
    const auto held = std::chrono::duration_cast<std::chrono::microseconds>(now - entry.arrival).count();
    stats_.lastHoldMicros = held > 0 ? static_cast<std::uint64_t>(held) : 0;
    const auto pts = entry.frame.timestampMicros;
    const auto difference = pts > audibleMicros ? pts - audibleMicros : audibleMicros - pts;
    stats_.haveEstimatedSkew = audibleMicros != 0 && pts != 0 && difference <= 2000000;
    stats_.estimatedSkewMicros = stats_.haveEstimatedSkew
        ? (pts >= audibleMicros ? static_cast<std::int64_t>(difference) : -static_cast<std::int64_t>(difference)) : 0;
}
bool AudioVideoQueue::Take(std::uint64_t audibleMicros, Clock::time_point now, VideoFrame& frame) {
    if (frames_.empty()) return false;
    const auto pts = frames_.back().frame.timestampMicros;
    const auto difference = pts > audibleMicros ? pts - audibleMicros : audibleMicros - pts;
    if (audibleMicros == 0 || pts == 0 || difference > 2000000) {
        RecordPresentation(frames_.back(),audibleMicros,now);
        stats_.coalescedFrames += frames_.size() - 1;
        frame = std::move(frames_.back().frame);
        frames_.clear(); overflowPending_ = false; return true;
    }
    bool found = false, deadlineRelease = false, overflowRelease = false;
    while (!frames_.empty()) {
        const auto& entry = frames_.front();
        const auto picturePts = entry.frame.timestampMicros;
        const bool matching = picturePts <= audibleMicros || picturePts - audibleMicros <= 10000;
        const bool expired = now - entry.arrival >= std::chrono::milliseconds(100);
        if (!matching && !expired && !overflowPending_) break;
        deadlineRelease = !matching && expired;
        overflowRelease = !matching && !expired && overflowPending_;
        overflowPending_ = false;
        if (found) ++stats_.coalescedFrames;
        RecordPresentation(entry,audibleMicros,now);
        frame = std::move(frames_.front().frame); frames_.pop_front(); found = true;
        // Coalesce only pictures already due. Keep future pictures and their
        // original arrival times, so deadline fallback maintains video cadence.
    }
    if (deadlineRelease) ++stats_.deadlineReleases;
    if (overflowRelease) ++stats_.overflowReleases;
    return found;
}
} // namespace phonecast::core::audio
