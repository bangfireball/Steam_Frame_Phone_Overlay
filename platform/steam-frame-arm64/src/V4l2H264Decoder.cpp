#include "phonecast/platform/steamframe/V4l2H264Decoder.h"

#include <linux/videodev2.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace phonecast::platform::steamframe {
namespace {

constexpr std::uint32_t kOutputBufferCount = 8;
constexpr std::uint32_t kCaptureBufferCount = 8;
constexpr std::uint32_t kMaximumPlanes = VIDEO_MAX_PLANES;
constexpr std::uint32_t kMinimumCompressedBufferSize = 2U * 1024U * 1024U;

bool Ioctl(int fd, unsigned long request, void* argument) {
    int result;
    do {
        result = ioctl(fd, request, argument);
    } while (result < 0 && errno == EINTR);
    return result == 0;
}

std::string SystemError(const std::string& operation) {
    return operation + ": " + std::strerror(errno);
}

std::string FourCc(std::uint32_t value) {
    std::string result(4, ' ');
    result[0] = static_cast<char>(value & 0xffU);
    result[1] = static_cast<char>((value >> 8U) & 0xffU);
    result[2] = static_cast<char>((value >> 16U) & 0xffU);
    result[3] = static_cast<char>((value >> 24U) & 0xffU);
    return result;
}

std::uint8_t ClampColor(int value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

struct MappedPlane {
    void* address{MAP_FAILED};
    std::size_t length{};
};

struct MappedBuffer {
    std::array<MappedPlane, kMaximumPlanes> planes{};
    std::uint32_t planeCount{};
    bool queued{};
};

void Unmap(std::vector<MappedBuffer>& buffers) noexcept {
    for (auto& buffer : buffers) {
        for (std::uint32_t plane = 0; plane < buffer.planeCount; ++plane) {
            if (buffer.planes[plane].address != MAP_FAILED) {
                munmap(buffer.planes[plane].address, buffer.planes[plane].length);
                buffer.planes[plane].address = MAP_FAILED;
            }
        }
    }
    buffers.clear();
}

bool SupportsH264M2m(int fd) {
    v4l2_capability capability{};
    if (!Ioctl(fd, VIDIOC_QUERYCAP, &capability)) return false;
    const std::uint32_t caps = (capability.capabilities & V4L2_CAP_DEVICE_CAPS) != 0U
        ? capability.device_caps : capability.capabilities;
    if ((caps & V4L2_CAP_STREAMING) == 0U ||
        (caps & V4L2_CAP_VIDEO_M2M_MPLANE) == 0U) return false;

    v4l2_fmtdesc description{};
    description.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    for (description.index = 0; Ioctl(fd, VIDIOC_ENUM_FMT, &description);
         ++description.index) {
        if (description.pixelformat == V4L2_PIX_FMT_H264) return true;
    }
    return false;
}

}  // namespace

struct V4l2H264Decoder::Implementation {
    explicit Implementation(std::string requested) : requestedDevice(std::move(requested)) {}

    bool OpenDevice(std::string& error) {
        std::vector<std::string> candidates;
        if (!requestedDevice.empty()) {
            candidates.push_back(requestedDevice);
        } else {
            for (int index = 0; index < 64; ++index)
                candidates.push_back("/dev/video" + std::to_string(index));
        }
        for (const auto& candidate : candidates) {
            if (!std::filesystem::exists(candidate)) continue;
            const int candidateFd = open(candidate.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
            if (candidateFd < 0) continue;
            if (SupportsH264M2m(candidateFd)) {
                fd = candidateFd;
                activeDevice = candidate;
                return true;
            }
            close(candidateFd);
        }
        error = requestedDevice.empty()
            ? "No stateful V4L2 H.264 M2M decoder was found under /dev/video*."
            : "The requested video device does not expose streaming H.264 M2M decode: " +
                  requestedDevice;
        return false;
    }

    bool MapBuffers(v4l2_buf_type type, std::uint32_t requestedCount,
                    std::vector<MappedBuffer>& target, std::string& error) {
        v4l2_requestbuffers request{};
        request.count = requestedCount;
        request.type = type;
        request.memory = V4L2_MEMORY_MMAP;
        if (!Ioctl(fd, VIDIOC_REQBUFS, &request) || request.count == 0U) {
            error = SystemError("VIDIOC_REQBUFS");
            return false;
        }
        target.resize(request.count);
        for (std::uint32_t index = 0; index < request.count; ++index) {
            std::array<v4l2_plane, kMaximumPlanes> planes{};
            v4l2_buffer buffer{};
            buffer.type = type;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = index;
            buffer.length = kMaximumPlanes;
            buffer.m.planes = planes.data();
            if (!Ioctl(fd, VIDIOC_QUERYBUF, &buffer)) {
                error = SystemError("VIDIOC_QUERYBUF");
                return false;
            }
            target[index].planeCount = buffer.length;
            for (std::uint32_t plane = 0; plane < buffer.length; ++plane) {
                void* address = mmap(nullptr, planes[plane].length,
                                     PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                     planes[plane].m.mem_offset);
                if (address == MAP_FAILED) {
                    error = SystemError("mmap V4L2 buffer");
                    return false;
                }
                target[index].planes[plane] = {address, planes[plane].length};
            }
        }
        return true;
    }

    bool ConfigureOutput(std::string& error) {
        v4l2_format format{};
        format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        format.fmt.pix_mp.width = configuredWidth;
        format.fmt.pix_mp.height = configuredHeight;
        format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
        format.fmt.pix_mp.field = V4L2_FIELD_NONE;
        format.fmt.pix_mp.num_planes = 1;
        format.fmt.pix_mp.plane_fmt[0].sizeimage = kMinimumCompressedBufferSize;
        if (!Ioctl(fd, VIDIOC_S_FMT, &format)) {
            error = SystemError("Setting V4L2 H.264 output format");
            return false;
        }
        if (format.fmt.pix_mp.pixelformat != V4L2_PIX_FMT_H264) {
            error = "V4L2 decoder rejected H.264 and selected " +
                    FourCc(format.fmt.pix_mp.pixelformat) + ".";
            return false;
        }
        if (!MapBuffers(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, kOutputBufferCount,
                        outputBuffers, error)) return false;
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        if (!Ioctl(fd, VIDIOC_STREAMON, &type)) {
            error = SystemError("Starting V4L2 compressed-input stream");
            return false;
        }
        outputStreaming = true;
        return true;
    }

    bool ConfigureCapture(std::string& error) {
        v4l2_format format{};
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (!Ioctl(fd, VIDIOC_G_FMT, &format)) {
            error = SystemError("Reading V4L2 capture format");
            return false;
        }
        auto pixelFormat = format.fmt.pix_mp.pixelformat;
        if (pixelFormat != V4L2_PIX_FMT_NV12 && pixelFormat != V4L2_PIX_FMT_NV12M) {
            std::uint32_t selectedFormat = 0;
            v4l2_fmtdesc description{};
            description.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            for (description.index = 0; Ioctl(fd, VIDIOC_ENUM_FMT, &description);
                 ++description.index) {
                if (description.pixelformat == V4L2_PIX_FMT_NV12)
                    selectedFormat = V4L2_PIX_FMT_NV12;
                else if (description.pixelformat == V4L2_PIX_FMT_NV12M && selectedFormat == 0U)
                    selectedFormat = V4L2_PIX_FMT_NV12M;
            }
            if (selectedFormat == 0U) {
                error = "V4L2 decoder produced unsupported capture format " +
                        FourCc(pixelFormat) + "; NV12/NV12M is required.";
                return false;
            }
            format.fmt.pix_mp.pixelformat = selectedFormat;
            if (!Ioctl(fd, VIDIOC_S_FMT, &format)) {
                error = SystemError("Selecting linear V4L2 NV12 capture format");
                return false;
            }
            pixelFormat = format.fmt.pix_mp.pixelformat;
            if (pixelFormat != V4L2_PIX_FMT_NV12 && pixelFormat != V4L2_PIX_FMT_NV12M) {
                error = "V4L2 decoder could not select a linear NV12 capture format.";
                return false;
            }
        }
        captureWidth = format.fmt.pix_mp.width;
        captureHeight = format.fmt.pix_mp.height;
        capturePixelFormat = pixelFormat;
        capturePlaneCount = format.fmt.pix_mp.num_planes;
        lumaStride = format.fmt.pix_mp.plane_fmt[0].bytesperline;
        chromaStride = capturePlaneCount > 1U
            ? format.fmt.pix_mp.plane_fmt[1].bytesperline : lumaStride;
        if (captureWidth == 0U || captureHeight == 0U || lumaStride == 0U) {
            error = "V4L2 decoder returned invalid capture dimensions or stride.";
            return false;
        }
        if (!MapBuffers(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, kCaptureBufferCount,
                        captureBuffers, error)) return false;
        for (std::uint32_t index = 0; index < captureBuffers.size(); ++index) {
            std::array<v4l2_plane, kMaximumPlanes> planes{};
            v4l2_buffer buffer{};
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = index;
            buffer.length = captureBuffers[index].planeCount;
            buffer.m.planes = planes.data();
            for (std::uint32_t plane = 0; plane < buffer.length; ++plane)
                planes[plane].length = captureBuffers[index].planes[plane].length;
            if (!Ioctl(fd, VIDIOC_QBUF, &buffer)) {
                error = SystemError("Queueing V4L2 capture buffer");
                return false;
            }
            captureBuffers[index].queued = true;
        }
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (!Ioctl(fd, VIDIOC_STREAMON, &type)) {
            error = SystemError("Starting V4L2 decoded-frame stream");
            return false;
        }
        captureStreaming = true;
        return true;
    }

    void ReclaimOutput() {
        if (!outputStreaming) return;
        while (true) {
            std::array<v4l2_plane, kMaximumPlanes> planes{};
            v4l2_buffer buffer{};
            buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.length = kMaximumPlanes;
            buffer.m.planes = planes.data();
            if (!Ioctl(fd, VIDIOC_DQBUF, &buffer)) break;
            if (buffer.index < outputBuffers.size()) outputBuffers[buffer.index].queued = false;
        }
    }

    bool HandleEvents(std::string& error) {
        while (true) {
            v4l2_event event{};
            if (!Ioctl(fd, VIDIOC_DQEVENT, &event)) {
                // Drivers differ on how an empty event queue is reported.
                // Qualcomm iris returns ENOENT while other V4L2 drivers use
                // EAGAIN; both mean there is no event to process yet.
                if (errno == EAGAIN || errno == ENOENT) return true;
                error = SystemError("Reading V4L2 decoder event");
                return false;
            }
            if (event.type == V4L2_EVENT_SOURCE_CHANGE && !captureStreaming) {
                if (!ConfigureCapture(error)) return false;
            }
        }
    }

    bool QueueInput(const std::vector<std::uint8_t>& accessUnit,
                    std::uint64_t timestampMicros, std::string& error) {
        ReclaimOutput();
        auto available = std::find_if(outputBuffers.begin(), outputBuffers.end(),
                                      [](const MappedBuffer& buffer) { return !buffer.queued; });
        if (available == outputBuffers.end()) {
            pollfd descriptor{fd, POLLOUT | POLLPRI, 0};
            poll(&descriptor, 1, 20);
            if (!HandleEvents(error)) return false;
            ReclaimOutput();
            available = std::find_if(outputBuffers.begin(), outputBuffers.end(),
                                     [](const MappedBuffer& buffer) { return !buffer.queued; });
        }
        if (available == outputBuffers.end()) {
            error = "V4L2 decoder did not return a compressed-input buffer in time.";
            return false;
        }
        if (accessUnit.size() > available->planes[0].length) {
            error = "H.264 access unit exceeds the V4L2 compressed-input buffer.";
            return false;
        }
        std::memcpy(available->planes[0].address, accessUnit.data(), accessUnit.size());
        const auto index = static_cast<std::uint32_t>(
            std::distance(outputBuffers.begin(), available));
        std::array<v4l2_plane, kMaximumPlanes> planes{};
        planes[0].bytesused = static_cast<std::uint32_t>(accessUnit.size());
        planes[0].length = available->planes[0].length;
        v4l2_buffer buffer{};
        buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = index;
        buffer.length = 1;
        buffer.m.planes = planes.data();
        buffer.timestamp.tv_sec = static_cast<long>(timestampMicros / 1'000'000U);
        buffer.timestamp.tv_usec = static_cast<long>(timestampMicros % 1'000'000U);
        if (!Ioctl(fd, VIDIOC_QBUF, &buffer)) {
            error = SystemError("Queueing H.264 access unit");
            return false;
        }
        available->queued = true;
        return true;
    }

    bool ConvertCapture(const v4l2_buffer& buffer, const v4l2_plane* planes,
                        core::VideoFrame& frame, std::string& error) {
        if (buffer.index >= captureBuffers.size()) {
            error = "V4L2 returned an invalid capture-buffer index.";
            return false;
        }
        const auto& mapped = captureBuffers[buffer.index];
        const auto* yPlane = static_cast<const std::uint8_t*>(mapped.planes[0].address) +
            planes[0].data_offset;
        const std::uint8_t* uvPlane = nullptr;
        if (capturePixelFormat == V4L2_PIX_FMT_NV12M) {
            if (mapped.planeCount < 2U) {
                error = "V4L2 NV12M capture buffer omitted its chroma plane.";
                return false;
            }
            uvPlane = static_cast<const std::uint8_t*>(mapped.planes[1].address) +
                planes[1].data_offset;
        } else {
            uvPlane = static_cast<const std::uint8_t*>(mapped.planes[0].address) +
                planes[0].data_offset + static_cast<std::size_t>(lumaStride) * captureHeight;
        }
        const std::uint32_t visibleWidth = std::min(configuredWidth, captureWidth);
        const std::uint32_t visibleHeight = std::min(configuredHeight, captureHeight);
        frame.width = visibleWidth;
        frame.height = visibleHeight;
        frame.format = core::PixelFormat::Rgba8;
        frame.sequence = buffer.sequence;
        frame.timestampMicros = buffer.timestamp.tv_sec >= 0 && buffer.timestamp.tv_usec >= 0
            ? static_cast<std::uint64_t>(buffer.timestamp.tv_sec) * 1000000U +
                static_cast<std::uint64_t>(buffer.timestamp.tv_usec) : 0;
        frame.pixels.resize(static_cast<std::size_t>(visibleWidth) * visibleHeight * 4U);
        for (std::uint32_t y = 0; y < visibleHeight; ++y) {
            for (std::uint32_t x = 0; x < visibleWidth; ++x) {
                const int luminance = static_cast<int>(yPlane[
                    static_cast<std::size_t>(y) * lumaStride + x]);
                const std::size_t chromaOffset = static_cast<std::size_t>(y / 2U) *
                    chromaStride + (x & ~1U);
                const int u = static_cast<int>(uvPlane[chromaOffset]) - 128;
                const int v = static_cast<int>(uvPlane[chromaOffset + 1U]) - 128;
                const int c = std::max(0, luminance - 16);
                const std::size_t target =
                    (static_cast<std::size_t>(y) * visibleWidth + x) * 4U;
                frame.pixels[target] = ClampColor((298 * c + 409 * v + 128) >> 8);
                frame.pixels[target + 1U] = ClampColor(
                    (298 * c - 100 * u - 208 * v + 128) >> 8);
                frame.pixels[target + 2U] = ClampColor((298 * c + 516 * u + 128) >> 8);
                frame.pixels[target + 3U] = 255U;
            }
        }
        return true;
    }

    bool DrainCapture(core::VideoFrame& frame, bool& producedFrame, std::string& error) {
        if (!captureStreaming) return true;
        while (true) {
            std::array<v4l2_plane, kMaximumPlanes> planes{};
            v4l2_buffer buffer{};
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.length = kMaximumPlanes;
            buffer.m.planes = planes.data();
            if (!Ioctl(fd, VIDIOC_DQBUF, &buffer)) {
                if (errno == EAGAIN) return true;
                error = SystemError("Dequeuing decoded V4L2 frame");
                return false;
            }
            if (buffer.index < captureBuffers.size()) captureBuffers[buffer.index].queued = false;
            if ((buffer.flags & V4L2_BUF_FLAG_ERROR) == 0U && planes[0].bytesused > 0U) {
                if (!ConvertCapture(buffer, planes.data(), frame, error)) return false;
                producedFrame = true;
            }
            if (!Ioctl(fd, VIDIOC_QBUF, &buffer)) {
                error = SystemError("Requeueing V4L2 capture buffer");
                return false;
            }
            if (buffer.index < captureBuffers.size()) captureBuffers[buffer.index].queued = true;
        }
    }

    void Stop() noexcept {
        if (fd >= 0) {
            if (captureStreaming) {
                v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
                ioctl(fd, VIDIOC_STREAMOFF, &type);
            }
            if (outputStreaming) {
                v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
                ioctl(fd, VIDIOC_STREAMOFF, &type);
            }
        }
        captureStreaming = false;
        outputStreaming = false;
        Unmap(captureBuffers);
        Unmap(outputBuffers);
        if (fd >= 0) close(fd);
        fd = -1;
        activeDevice.clear();
        captureWidth = captureHeight = 0;
    }

    std::string requestedDevice;
    std::string activeDevice;
    int fd{-1};
    std::uint32_t configuredWidth{};
    std::uint32_t configuredHeight{};
    std::uint32_t captureWidth{};
    std::uint32_t captureHeight{};
    std::uint32_t capturePixelFormat{};
    std::uint32_t capturePlaneCount{};
    std::uint32_t lumaStride{};
    std::uint32_t chromaStride{};
    bool outputStreaming{};
    bool captureStreaming{};
    std::vector<MappedBuffer> outputBuffers;
    std::vector<MappedBuffer> captureBuffers;
};

V4l2H264Decoder::V4l2H264Decoder(std::string devicePath)
    : implementation_(std::make_unique<Implementation>(std::move(devicePath))) {}
V4l2H264Decoder::~V4l2H264Decoder() { Stop(); }

bool V4l2H264Decoder::Start(std::uint32_t width, std::uint32_t height,
                            std::string& error) {
    auto& state = *implementation_;
    state.Stop();
    if (width == 0U || height == 0U) {
        error = "V4L2 decoder dimensions must be non-zero.";
        return false;
    }
    state.configuredWidth = width;
    state.configuredHeight = height;
    if (!state.OpenDevice(error)) {
        state.Stop();
        return false;
    }
    v4l2_event_subscription subscription{};
    subscription.type = V4L2_EVENT_SOURCE_CHANGE;
    if (!Ioctl(state.fd, VIDIOC_SUBSCRIBE_EVENT, &subscription)) {
        error = SystemError("Subscribing to V4L2 source-change events");
        state.Stop();
        return false;
    }
    if (!state.ConfigureOutput(error)) {
        state.Stop();
        return false;
    }
    error.clear();
    return true;
}

bool V4l2H264Decoder::Submit(const std::vector<std::uint8_t>& accessUnit,
                             std::uint64_t timestampMicros,
                             core::VideoFrame& frame, bool& producedFrame,
                             std::string& error) {
    auto& state = *implementation_;
    producedFrame = false;
    if (state.fd < 0) {
        error = "V4L2 decoder is not started.";
        return false;
    }
    if (accessUnit.empty()) {
        error = "Cannot submit an empty H.264 access unit.";
        return false;
    }
    if (!state.HandleEvents(error) ||
        !state.QueueInput(accessUnit, timestampMicros, error)) return false;
    pollfd descriptor{state.fd, POLLIN | POLLPRI, 0};
    poll(&descriptor, 1, 8);
    if (!state.HandleEvents(error) ||
        !state.DrainCapture(frame, producedFrame, error)) return false;
    error.clear();
    return true;
}

void V4l2H264Decoder::Stop() noexcept { implementation_->Stop(); }
const std::string& V4l2H264Decoder::DevicePath() const noexcept {
    return implementation_->activeDevice;
}

}  // namespace phonecast::platform::steamframe
