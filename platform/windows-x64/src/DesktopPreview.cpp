#include "phonecast/platform/windows/DesktopPreview.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <vector>

namespace phonecast::platform::windows {

struct DesktopPreview::Implementation {
    HWND window{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> bgra;
    bool running{};

    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        auto* state = reinterpret_cast<Implementation*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            state = static_cast<Implementation*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        }
        if (message == WM_CLOSE) {
            if (state != nullptr) state->running = false;
            DestroyWindow(window);
            return 0;
        }
        if (message == WM_DESTROY) {
            if (state != nullptr) {
                state->window = nullptr;
                state->running = false;
            }
            return 0;
        }
        if (message == WM_PAINT && state != nullptr) {
            PAINTSTRUCT paint{};
            HDC device = BeginPaint(window, &paint);
            RECT client{};
            GetClientRect(window, &client);
            FillRect(device, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            if (!state->bgra.empty() && state->width > 0 && state->height > 0) {
                const int clientWidth = client.right - client.left;
                const int clientHeight = client.bottom - client.top;
                const double scale = std::min(static_cast<double>(clientWidth) / state->width,
                                              static_cast<double>(clientHeight) / state->height);
                const int drawWidth = static_cast<int>(state->width * scale);
                const int drawHeight = static_cast<int>(state->height * scale);
                const int x = (clientWidth - drawWidth) / 2;
                const int y = (clientHeight - drawHeight) / 2;
                BITMAPINFO bitmap{};
                bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bitmap.bmiHeader.biWidth = static_cast<LONG>(state->width);
                bitmap.bmiHeader.biHeight = -static_cast<LONG>(state->height);
                bitmap.bmiHeader.biPlanes = 1;
                bitmap.bmiHeader.biBitCount = 32;
                bitmap.bmiHeader.biCompression = BI_RGB;
                SetStretchBltMode(device, HALFTONE);
                StretchDIBits(device, x, y, drawWidth, drawHeight, 0, 0,
                              static_cast<int>(state->width), static_cast<int>(state->height),
                              state->bgra.data(), &bitmap, DIB_RGB_COLORS, SRCCOPY);
            }
            EndPaint(window, &paint);
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }
};

DesktopPreview::DesktopPreview() : implementation_(std::make_unique<Implementation>()) {}
DesktopPreview::~DesktopPreview() { Stop(); }

bool DesktopPreview::Start(std::string& error) {
    auto& state = *implementation_;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = Implementation::WindowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"PhoneCastDesktopPreview";
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    if (RegisterClassW(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        error = "Could not register the desktop preview window class.";
        return false;
    }
    state.window = CreateWindowExW(0, windowClass.lpszClassName,
        L"PhoneCast Receiver - waiting for phone", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 720, nullptr, nullptr, instance, &state);
    if (state.window == nullptr) {
        error = "Could not create the desktop preview window.";
        return false;
    }
    state.running = true;
    error.clear();
    return true;
}

bool DesktopPreview::PumpEvents() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return implementation_->running;
}

void DesktopPreview::Present(const core::VideoFrame& frame) {
    if (!frame.IsValid()) return;
    auto& state = *implementation_;
    state.width = frame.width;
    state.height = frame.height;
    state.bgra.resize(frame.pixels.size());
    for (std::size_t offset = 0; offset < frame.pixels.size(); offset += 4) {
        state.bgra[offset] = frame.pixels[offset + 2];
        state.bgra[offset + 1] = frame.pixels[offset + 1];
        state.bgra[offset + 2] = frame.pixels[offset];
        state.bgra[offset + 3] = 255;
    }
    if (state.window != nullptr) InvalidateRect(state.window, nullptr, FALSE);
}

void DesktopPreview::SetTitle(const std::string& title) {
    if (implementation_->window == nullptr) return;
    const int length = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nullptr, 0);
    if (length <= 0) return;
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wide.data(), length);
    SetWindowTextW(implementation_->window, wide.c_str());
}

void DesktopPreview::Stop() noexcept {
    auto& state = *implementation_;
    if (state.window != nullptr) DestroyWindow(state.window);
    state.window = nullptr;
    state.running = false;
    state.bgra.clear();
}

}  // namespace phonecast::platform::windows
