# PhoneCast VR architecture

## Sprint 1 baseline

The receiver is divided into portable contracts, application orchestration, and an OpenVR adapter:

```text
GeneratedVideoSource (Core)
          |
          v
Receiver orchestration (apps/receiver)
          |
          v
IOverlayRenderer (VR contract)
          |
          v
OpenVrOverlayRenderer (platform/openvr)
          |
          v
SteamVR compositor
```

## Dependency rules

- `core` contains configuration, logging, frame types, generated video, and future transport/decoder/input contracts. It does not include OpenVR, Win32, DirectX, or operating-system headers.
- `vr` defines renderer-facing VR contracts and depends only on Core frame types.
- `apps/receiver` coordinates a video source and overlay renderer through interfaces.
- `platform/openvr` is the only production component that includes `openvr.h`.
- The executable composition root selects concrete implementations.
- Platform packaging may depend on Windows x64 or Linux ARM64 details; reusable logic may not.

## Current interfaces

- `IVideoSource` supplies decoded or generated `VideoFrame` objects.
- `IVideoDecoder` defines the future encoded-packet boundary; Sprint 1 has no codec.
- `INetworkTransport` defines a future byte transport; Sprint 1 opens no sockets.
- `IInputProvider` and `IRemoteInputSender` define normalized platform-independent input boundaries.
- `IOverlayRenderer` owns VR startup, frame submission, event pumping, and shutdown.
- `ILogger` receives severity, subsystem, and message fields.

Interfaces are deliberately small. They will be revised when the first real transport and decoder provide concrete requirements.

## Frame ownership and format

`VideoFrame` currently owns tightly packed RGBA8 CPU memory. The generated source produces a moving vertical band and horizontal progress line. The OpenVR backend uploads each frame with `SetOverlayRaw`.

This CPU upload is intentional for foundation validation. It is not the planned optimized video path. A later decoder/backend integration should support native GPU surfaces without forcing platform handles into Core.

## Runtime lifecycle

1. Parse and validate command-line configuration.
2. Initialize OpenVR as `VRApplication_Overlay`.
3. Register the adjacent application manifest when available.
4. Create and configure an HMD-relative regular overlay.
5. Generate and submit frames at the configured update rate.
6. Poll overlay and system quit events.
7. Hide/destroy the overlay and call `VR_Shutdown`.

`Receiver::Stop` is idempotent. The OpenVR renderer also stops in its destructor.

## Platform mapping

### Windows x64

Uses Valve's OpenVR client DLL and copies it beside `phonecast-receiver.exe`. No Win32 API is used by Core.

### Steam Frame Linux ARM64

Builds OpenVR from pinned source and links it statically. The same backend uses the manifest's required `binary_path_linux_arm`. Sprint 1 cross-builds this path; native visual validation remains hardware-dependent.

## Deferred work

Sprint 1 intentionally excludes Android capture, networking implementations, pairing, video codecs, remote control, placement modes, and GPU texture sharing.
