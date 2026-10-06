#include "phonecast/platform/windows/MfH264Decoder.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mftransform.h>
#include <wmcodecdsp.h>

#include <algorithm>
#include <sstream>

namespace phonecast::platform::windows {
namespace {

constexpr GUID kH264DecoderClsid = {0x62ce7e72, 0x4c71, 0x4d20,
    {0xb1, 0x5d, 0x45, 0x28, 0x31, 0xa8, 0x7d, 0x9d}};

// MF_LOW_LATENCY / CODECAPI_AVLowLatencyMode. Keep the GUID local, as with
// the decoder CLSID, for MinGW headers targeting pre-Windows-8 by default.
constexpr GUID kLowLatency = {0x9c27891a, 0xed7a, 0x40e1,
    {0x88, 0xe8, 0xb2, 0x27, 0x27, 0xa0, 0x24, 0xee}};

template <typename T> void Release(T*& value) {
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

std::string HResultMessage(const char* operation, HRESULT result) {
    std::ostringstream stream;
    stream << operation << " failed (HRESULT 0x" << std::hex
           << static_cast<unsigned long>(result) << ").";
    return stream.str();
}

std::uint8_t Clamp(int value) {
    return static_cast<std::uint8_t>(std::max(0, std::min(255, value)));
}

void Nv12ToRgba(const std::uint8_t* source, std::uint32_t visibleWidth,
                std::uint32_t visibleHeight, std::uint32_t stride,
                std::uint32_t codedHeight, std::vector<std::uint8_t>& output) {
    output.resize(static_cast<std::size_t>(visibleWidth) * visibleHeight * 4U);
    const std::uint8_t* uvPlane = source + static_cast<std::size_t>(stride) * codedHeight;
    for (std::uint32_t y = 0; y < visibleHeight; ++y) {
        for (std::uint32_t x = 0; x < visibleWidth; ++x) {
            const int luminance = static_cast<int>(source[y * stride + x]) - 16;
            const std::size_t uvOffset = static_cast<std::size_t>(y / 2U) * stride + (x & ~1U);
            const int u = static_cast<int>(uvPlane[uvOffset]) - 128;
            const int v = static_cast<int>(uvPlane[uvOffset + 1]) - 128;
            const int c = std::max(0, luminance);
            const std::size_t target = (static_cast<std::size_t>(y) * visibleWidth + x) * 4U;
            output[target] = Clamp((298 * c + 409 * v + 128) >> 8);
            output[target + 1] = Clamp((298 * c - 100 * u - 208 * v + 128) >> 8);
            output[target + 2] = Clamp((298 * c + 516 * u + 128) >> 8);
            output[target + 3] = 255;
        }
    }
}

}  // namespace

struct MfH264Decoder::Implementation {
    IMFTransform* transform{};
    std::uint32_t visibleWidth{};
    std::uint32_t visibleHeight{};
    std::uint32_t codedWidth{};
    std::uint32_t codedHeight{};
    std::uint64_t sequence{};
    bool comInitialized{};
    bool mediaFoundationStarted{};

    void Reset() noexcept {
        Release(transform);
        if (mediaFoundationStarted) {
            MFShutdown();
            mediaFoundationStarted = false;
        }
        if (comInitialized) {
            CoUninitialize();
            comInitialized = false;
        }
        visibleWidth = 0;
        visibleHeight = 0;
        codedWidth = 0;
        codedHeight = 0;
        sequence = 0;
    }
};

MfH264Decoder::MfH264Decoder() : implementation_(std::make_unique<Implementation>()) {}
MfH264Decoder::~MfH264Decoder() { Stop(); }

bool MfH264Decoder::Start(std::uint32_t width, std::uint32_t height, std::string& error) {
    Stop();
    auto& state = *implementation_;
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(result) && result != RPC_E_CHANGED_MODE) {
        error = HResultMessage("CoInitializeEx", result);
        return false;
    }
    state.comInitialized = result != RPC_E_CHANGED_MODE;
    result = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(result)) {
        error = HResultMessage("MFStartup", result);
        state.Reset();
        return false;
    }
    state.mediaFoundationStarted = true;
    result = CoCreateInstance(kH264DecoderClsid, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&state.transform));
    if (FAILED(result)) {
        error = HResultMessage("Creating the Media Foundation H.264 decoder", result);
        state.Reset();
        return false;
    }

    // Screen capture is not a continuous movie: a static screen can leave the
    // next input seconds away. Disable the decoder's internal look-ahead, not
    // just our receive queue. MF_LOW_LATENCY shares CODECAPI_AVLowLatencyMode's
    // GUID; the H.264 decoder expects a UINT32 value via IMFAttributes.
    IMFAttributes* attributes = nullptr;
    result = state.transform->GetAttributes(&attributes);
    if (SUCCEEDED(result)) result = attributes->SetUINT32(kLowLatency, TRUE);
    Release(attributes);
    if (FAILED(result)) {
        error = HResultMessage("Enabling low-latency H.264 decoding", result);
        state.Reset();
        return false;
    }

    IMFMediaType* inputType = nullptr;
    IMFMediaType* outputType = nullptr;
    result = MFCreateMediaType(&inputType);
    if (SUCCEEDED(result)) result = inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(result)) result = inputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    if (SUCCEEDED(result)) result = MFSetAttributeSize(inputType, MF_MT_FRAME_SIZE, width, height);
    if (SUCCEEDED(result)) result = MFSetAttributeRatio(inputType, MF_MT_FRAME_RATE, 30, 1);
    if (SUCCEEDED(result)) {
        result = inputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    }
    if (SUCCEEDED(result)) result = state.transform->SetInputType(0, inputType, 0);

    if (SUCCEEDED(result)) result = MFCreateMediaType(&outputType);
    if (SUCCEEDED(result)) result = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    if (SUCCEEDED(result)) result = outputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
    if (SUCCEEDED(result)) result = MFSetAttributeSize(outputType, MF_MT_FRAME_SIZE, width, height);
    if (SUCCEEDED(result)) result = MFSetAttributeRatio(outputType, MF_MT_FRAME_RATE, 30, 1);
    if (SUCCEEDED(result)) {
        result = outputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    }
    if (SUCCEEDED(result)) result = state.transform->SetOutputType(0, outputType, 0);
    Release(inputType);
    Release(outputType);
    if (FAILED(result)) {
        error = HResultMessage("Configuring the Media Foundation H.264 decoder", result);
        state.Reset();
        return false;
    }

    state.transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    state.transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    state.visibleWidth = width;
    state.visibleHeight = height;
    state.codedWidth = width;
    state.codedHeight = height;
    error.clear();
    return true;
}

bool MfH264Decoder::Submit(const std::vector<std::uint8_t>& accessUnit,
                           std::uint64_t timestampMicros, core::VideoFrame& frame,
                           bool& producedFrame, std::string& error) {
    producedFrame = false;
    auto& state = *implementation_;
    if (state.transform == nullptr || accessUnit.empty()) {
        error = "H.264 decoder is not configured or received an empty access unit.";
        return false;
    }

    IMFSample* inputSample = nullptr;
    IMFMediaBuffer* inputBuffer = nullptr;
    HRESULT result = MFCreateSample(&inputSample);
    if (SUCCEEDED(result)) {
        result = MFCreateMemoryBuffer(static_cast<DWORD>(accessUnit.size()), &inputBuffer);
    }
    BYTE* target = nullptr;
    if (SUCCEEDED(result)) result = inputBuffer->Lock(&target, nullptr, nullptr);
    if (SUCCEEDED(result)) {
        std::copy(accessUnit.begin(), accessUnit.end(), target);
        inputBuffer->Unlock();
        result = inputBuffer->SetCurrentLength(static_cast<DWORD>(accessUnit.size()));
    }
    if (SUCCEEDED(result)) result = inputSample->AddBuffer(inputBuffer);
    if (SUCCEEDED(result)) result = inputSample->SetSampleTime(static_cast<LONGLONG>(timestampMicros * 10U));
    if (SUCCEEDED(result)) result = inputSample->SetSampleDuration(333333);
    if (SUCCEEDED(result)) result = state.transform->ProcessInput(0, inputSample, 0);
    Release(inputBuffer);
    Release(inputSample);
    if (FAILED(result)) {
        error = HResultMessage("Submitting an H.264 access unit", result);
        return false;
    }

    IMFSample* outputSample = nullptr;
    for (int attempt = 0; attempt < 2; ++attempt) {
        MFT_OUTPUT_STREAM_INFO streamInfo{};
        result = state.transform->GetOutputStreamInfo(0, &streamInfo);
        if (FAILED(result)) {
            error = HResultMessage("Reading decoder output requirements", result);
            return false;
        }
        IMFMediaBuffer* outputBuffer = nullptr;
        result = MFCreateSample(&outputSample);
        if (SUCCEEDED(result)) {
            result = MFCreate2DMediaBuffer(state.codedWidth, state.codedHeight,
                                           MFVideoFormat_NV12.Data1, FALSE, &outputBuffer);
        }
        if (SUCCEEDED(result)) result = outputSample->AddBuffer(outputBuffer);
        Release(outputBuffer);
        if (FAILED(result)) {
            Release(outputSample);
            error = HResultMessage("Allocating decoder output", result);
            return false;
        }

        MFT_OUTPUT_DATA_BUFFER output{};
        output.dwStreamID = 0;
        output.pSample = outputSample;
        DWORD status = 0;
        result = state.transform->ProcessOutput(0, 1, &output, &status);
        if (output.pEvents != nullptr) output.pEvents->Release();
        if (result != MF_E_TRANSFORM_STREAM_CHANGE) break;

        Release(outputSample);
        HRESULT typeResult = MF_E_INVALIDMEDIATYPE;
        for (DWORD typeIndex = 0;; ++typeIndex) {
            IMFMediaType* availableType = nullptr;
            const HRESULT availableResult =
                state.transform->GetOutputAvailableType(0, typeIndex, &availableType);
            if (availableResult == MF_E_NO_MORE_TYPES) break;
            if (FAILED(availableResult)) {
                typeResult = availableResult;
                break;
            }
            GUID subtype{};
            if (SUCCEEDED(availableType->GetGUID(MF_MT_SUBTYPE, &subtype)) &&
                subtype == MFVideoFormat_NV12) {
                UINT32 decodedWidth = 0;
                UINT32 decodedHeight = 0;
                if (SUCCEEDED(MFGetAttributeSize(availableType, MF_MT_FRAME_SIZE,
                                                 &decodedWidth, &decodedHeight)) &&
                    decodedWidth > 0 && decodedHeight > 0) {
                    state.codedWidth = decodedWidth;
                    state.codedHeight = decodedHeight;
                }
                typeResult = state.transform->SetOutputType(0, availableType, 0);
                Release(availableType);
                break;
            }
            Release(availableType);
        }
        if (FAILED(typeResult)) {
            error = HResultMessage("Accepting the decoder's changed output format", typeResult);
            return false;
        }
    }
    if (result == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        Release(outputSample);
        error.clear();
        return true;
    }
    if (FAILED(result)) {
        Release(outputSample);
        error = HResultMessage("Receiving a decoded video frame", result);
        return false;
    }

    IMFMediaBuffer* decodedBuffer = nullptr;
    IMF2DBuffer* buffer2d = nullptr;
    result = outputSample->GetBufferByIndex(0, &decodedBuffer);
    if (SUCCEEDED(result)) result = decodedBuffer->QueryInterface(IID_PPV_ARGS(&buffer2d));
    BYTE* decoded = nullptr;
    LONG pitch = 0;
    if (SUCCEEDED(result)) result = buffer2d->Lock2D(&decoded, &pitch);
    if (SUCCEEDED(result) && pitch <= 0) result = E_UNEXPECTED;
    if (SUCCEEDED(result)) {
        frame.width = state.visibleWidth;
        frame.height = state.visibleHeight;
        frame.format = core::PixelFormat::Rgba8;
        frame.sequence = state.sequence++;
        LONGLONG pictureTime = 0;
        frame.timestampMicros = SUCCEEDED(outputSample->GetSampleTime(&pictureTime)) && pictureTime > 0
            ? static_cast<std::uint64_t>(pictureTime / 10) : 0;
        Nv12ToRgba(decoded, state.visibleWidth, state.visibleHeight,
                   static_cast<std::uint32_t>(pitch), state.codedHeight, frame.pixels);
        producedFrame = true;
    }
    if (decoded != nullptr) buffer2d->Unlock2D();
    Release(buffer2d);
    Release(decodedBuffer);
    Release(outputSample);
    if (FAILED(result)) {
        error = HResultMessage("Reading decoded NV12 frame", result);
        return false;
    }
    error.clear();
    return true;
}

void MfH264Decoder::Stop() noexcept {
    if (implementation_) implementation_->Reset();
}

}  // namespace phonecast::platform::windows
