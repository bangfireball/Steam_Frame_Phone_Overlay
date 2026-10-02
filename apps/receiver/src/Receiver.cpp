#include "phonecast/apps/Receiver.h"

namespace phonecast::apps {

Receiver::Receiver(core::IVideoSource& source, vr::IOverlayRenderer& renderer,
                   core::ILogger& logger)
    : source_(source), renderer_(renderer), logger_(logger) {}

bool Receiver::Start(const vr::OverlaySettings& settings, std::string& error) {
    if (started_) {
        error = "Receiver is already started.";
        return false;
    }
    logger_.Log(core::LogLevel::Info, "receiver", "Starting overlay backend.");
    if (!renderer_.Start(settings, error)) {
        logger_.Log(core::LogLevel::Error, "receiver", error);
        return false;
    }
    started_ = true;
    submittedFrames_ = 0;
    return true;
}

bool Receiver::Tick(std::string& error) {
    if (!started_) {
        error = "Receiver is not started.";
        return false;
    }
    if (!renderer_.PumpEvents()) {
        error.clear();
        return false;
    }
    core::VideoFrame frame;
    if (!source_.NextFrame(frame, error)) {
        logger_.Log(core::LogLevel::Error, "video-source", error);
        return false;
    }
    if (!frame.IsValid()) {
        error = "Video source returned an invalid frame.";
        logger_.Log(core::LogLevel::Error, "video-source", error);
        return false;
    }
    if (!renderer_.SubmitFrame(frame, error)) {
        logger_.Log(core::LogLevel::Error, "overlay", error);
        return false;
    }
    ++submittedFrames_;
    return true;
}

void Receiver::Stop() noexcept {
    if (!started_) return;
    renderer_.Stop();
    started_ = false;
    logger_.Log(core::LogLevel::Info, "receiver", "Stopped cleanly.");
}

}  // namespace phonecast::apps
