#include "phonecast/apps/Receiver.h"
#include "phonecast/core/config/AppConfig.h"
#include "phonecast/core/streaming/GeneratedVideoSource.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

class FakeLogger final : public phonecast::core::ILogger {
public:
    void Log(phonecast::core::LogLevel, std::string_view, std::string_view) override { ++entries; }
    int entries{0};
};

class FakeOverlay final : public phonecast::vr::IOverlayRenderer {
public:
    bool Start(const phonecast::vr::OverlaySettings&, std::string&) override {
        started = true;
        return startResult;
    }
    bool SubmitFrame(const phonecast::core::VideoFrame& frame, std::string&) override {
        ++frames;
        lastSequence = frame.sequence;
        return submitResult;
    }
    bool PumpEvents() override { return keepRunning; }
    void Stop() noexcept override { stopped = true; }
    bool startResult{true};
    bool submitResult{true};
    bool keepRunning{true};
    bool started{false};
    bool stopped{false};
    int frames{0};
    std::uint64_t lastSequence{0};
};

int failures = 0;
void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void TestConfig() {
    const auto defaults = phonecast::core::ParseCommandLine({});
    Check(defaults.status == phonecast::core::ParseStatus::Success, "default config parses");
    Check(defaults.config.framesPerSecond == 30, "default FPS is 30");

    const auto custom = phonecast::core::ParseCommandLine(
        {"--width", "800", "--height", "600", "--fps", "60", "--alpha", "0.5"});
    Check(custom.status == phonecast::core::ParseStatus::Success, "custom config parses");
    Check(custom.config.textureWidth == 800 && custom.config.textureHeight == 600,
          "custom dimensions apply");
    Check(custom.config.framesPerSecond == 60 && custom.config.overlayAlpha == 0.5F,
          "custom FPS and alpha apply");
    Check(phonecast::core::ParseCommandLine({"--fps", "0"}).status ==
              phonecast::core::ParseStatus::Error,
          "invalid FPS rejected");
    Check(phonecast::core::ParseCommandLine({"--unknown", "1"}).status ==
              phonecast::core::ParseStatus::Error,
          "unknown option rejected");
    Check(phonecast::core::ParseCommandLine({"--alpha", "nan"}).status ==
              phonecast::core::ParseStatus::Error,
          "non-finite values rejected");
}

void TestGeneratedFrames() {
    phonecast::core::GeneratedVideoSource source(64, 64);
    phonecast::core::VideoFrame first;
    phonecast::core::VideoFrame second;
    std::string error;
    Check(source.NextFrame(first, error) && first.IsValid(), "first generated frame is valid");
    Check(source.NextFrame(second, error) && second.IsValid(), "second generated frame is valid");
    Check(first.sequence == 0 && second.sequence == 1, "frame sequence increments");
    Check(first.pixels != second.pixels, "animation changes frame pixels");
}

void TestReceiverLifecycle() {
    phonecast::core::GeneratedVideoSource source(64, 64);
    FakeOverlay overlay;
    FakeLogger logger;
    phonecast::apps::Receiver receiver(source, overlay, logger);
    std::string error;
    Check(receiver.Start({}, error), "receiver starts");
    Check(receiver.Tick(error), "receiver submits a frame");
    Check(overlay.frames == 1 && receiver.SubmittedFrames() == 1, "submission is counted");
    receiver.Stop();
    receiver.Stop();
    Check(overlay.stopped, "renderer stops safely");

    FakeOverlay quittingOverlay;
    phonecast::apps::Receiver quittingReceiver(source, quittingOverlay, logger);
    Check(quittingReceiver.Start({}, error), "second receiver starts");
    quittingOverlay.keepRunning = false;
    error = "stale";
    Check(!quittingReceiver.Tick(error) && error.empty(), "runtime shutdown is clean");
    quittingReceiver.Stop();
}

}  // namespace

int main() {
    TestConfig();
    TestGeneratedFrames();
    TestReceiverLifecycle();
    if (failures == 0) std::cout << "All PhoneCast tests passed.\n";
    return failures == 0 ? 0 : 1;
}
