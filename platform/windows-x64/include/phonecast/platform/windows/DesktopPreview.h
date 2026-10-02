#pragma once

#include "phonecast/core/streaming/VideoFrame.h"

#include <memory>
#include <string>

namespace phonecast::platform::windows {

class DesktopPreview {
public:
    DesktopPreview();
    ~DesktopPreview();
    bool Start(std::string& error);
    bool PumpEvents();
    void Present(const core::VideoFrame& frame);
    void SetTitle(const std::string& title);
    void Stop() noexcept;

private:
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};

}  // namespace phonecast::platform::windows
