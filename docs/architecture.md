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

`Receiver::Stop` is idempotent. The OpenVR renderer also stops in its destructor. `VREvent_ProcessQuit` is informational about an arbitrary VR process and is deliberately not treated as a receiver shutdown request; doing so caused PhoneCast to exit when a VR scene application transitioned. Only explicit runtime/driver quit events stop the overlay.

## Platform mapping

### Windows x64

Uses Valve's OpenVR client DLL and copies it beside `phonecast-receiver.exe`. No Win32 API is used by Core.

### Steam Frame Linux ARM64

Builds OpenVR from pinned source and links it statically. The same backend uses the manifest's required `binary_path_linux_arm`. Sprint 1 cross-builds this path; native visual validation remains hardware-dependent.

## Sprint 3 streaming path

```text
Android MediaProjection → MediaCodec AVC → framed TCP
    → Windows receiver → Media Foundation H.264 decoder → RGBA8 → Win32 preview
```

The protocol serializer remains in Core and has no socket or operating-system headers. Android and the Windows receiver implement the same fixed, network-byte-order framing. The Windows decoder and desktop window are isolated under `platform/windows-x64`.

The desktop preview is a validation target, not the Sprint 4 VR rendering path. It currently converts Media Foundation NV12 output to CPU RGBA8. Steam Frame decoding and efficient decoder-to-compositor texture sharing remain separate backend work.

## Sprint 4 Windows VR path

```text
Android MediaProjection → MediaCodec AVC → framed TCP
    → TcpVideoServer → Media Foundation H.264 decoder → CPU RGBA8
    → reusable D3D11 texture → OpenVR SetOverlayTexture → VR compositor
```

The desktop and VR streaming executables share `TcpVideoServer` and `MfH264Decoder`. The desktop preview remains an independent diagnostic target. On Windows, `OpenVrOverlayRenderer` creates one D3D11 texture per stream resolution and updates it instead of repeatedly calling `SetOverlayRaw`; Linux ARM64 retains the raw fallback until its native decoder/rendering path is implemented.

The D3D11 device must be created on the DXGI adapter returned by OpenVR's `GetDXGIOutputInfo`. Using Windows' default adapter caused successful API calls but an invisible overlay on the multi-GPU validation PC. Each `UpdateSubresource` is followed by `ID3D11DeviceContext::Flush`; without that flush, the generated animation advanced only about once every five seconds through VRLink. With both corrections, the generated animation was visibly smooth and the physical Android screen appeared in-headset.

`OverlayController` contains platform-independent, bounded changes for visibility, width, X/Y offset, distance, opacity, placement mode, and reset. Placement supports head-, standing-world-, left-controller-, and right-controller-locked transforms. The Windows executable maps global Ctrl+Alt key combinations to those actions.

For world placement, the OpenVR backend snapshots the current HMD-relative panel into SteamVR standing space, so switching modes does not move the panel to the room origin. Controller modes currently use role-based tracked-device positions with a short controller-local distance. Their panel orientation is recomputed against the HMD pose so the screen stays upright and faces the user instead of inheriting the controller's pitch/roll. Physical testing over a PC SteamVR game confirmed this functions, but it is not the desired final ergonomic pose. Sprint 5.1 therefore defers configurable per-hand offsets, tilt/yaw, scale, and selectable controller-relative/billboard/wrist-style orientation behavior. The Windows-hosted overlay disappears when a headset-native standalone game takes over; this is a compositor/backend boundary rather than a placement-model failure and remains native Sprint 11 work. OpenVR mouse input also permits a controller laser trigger to grab the visible panel; release converts its current pose to a world anchor. Renderer-originated placement updates flow back through `IOverlayRenderer::TakeSettingsUpdate` without exposing OpenVR types.

`OverlaySettingsStore` persists the portable placement and appearance model in a versioned text file. The stream receiver stores it under `%LOCALAPPDATA%\PhoneCastVR` by default and accepts `--settings` for an explicit path. Stream aspect-ratio adjustment remains presentation-only and is not written over the user's base width.

Media Foundation may report an aligned coded width that exceeds the phone's visible width. The decoder now allocates using coded dimensions while converting and exposing only the original visible dimensions. This is intended to remove the previously observed right-edge green padding and requires physical revalidation.

The current path still performs decoder NV12 → CPU RGBA → D3D11 upload. Direct Media Foundation/DXGI surface conversion and synchronization remain performance work; platform handles have not been added to Core merely to anticipate that optimization.

## Deferred work

Encrypted pairing, automatic discovery, remote control, Sprint 5.1 controller-placement calibration, standalone native-overlay validation, and native decoder-to-GPU surface sharing remain deferred. The custom TCP transport is subject to head-of-line blocking and must be measured on real Wi-Fi before it is treated as a long-term choice.
