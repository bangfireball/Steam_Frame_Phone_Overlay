# Steam Frame plugin research: Frame Perf Overlay and Frame Mic Tuner

Research date: 2026-10-03  
Scope: native Steam Frame rendering, dashboard interaction, lifecycle, placement, diagnostics, packaging, and lessons for PhoneCast VR

Repositories reviewed:

- [`sasaken1102r/frame-perf-overlay`](https://github.com/sasaken1102r/frame-perf-overlay) at commit [`8baaf7e`](https://github.com/sasaken1102r/frame-perf-overlay/tree/8baaf7e753ebd449c035c1d11a9830eba04d0201), release line v0.3.0
- [`sasaken1102r/frame-mic-tuner`](https://github.com/sasaken1102r/frame-mic-tuner) at commit [`d1e8c83`](https://github.com/sasaken1102r/frame-mic-tuner/tree/d1e8c830b00928ab94a44e1d5323613af95263c6), release line v0.2.0

Both projects are MIT-licensed. This review examined their READMEs, build/install files, systemd units, OpenVR adapters, Vulkan texture implementation, event loops, placement code, tests, and relevant application logic. Their authors report physical Steam Frame use; PhoneCast has not independently run either plugin or reproduced their measurements.

## Executive summary

The most valuable finding is a working native Linux ARM64 OpenVR-to-Vulkan texture path. Both plugins run directly on Steam Frame, ask OpenVR for the required Vulkan extensions and compositor GPU, upload RGBA through persistent Vulkan resources, and submit with `SetOverlayTexture`. This directly addresses PhoneCast's known Linux `SetOverlayRaw` flicker and is the best available reference for the first Sprint 11 renderer prototype.

Other strong design lessons are:

1. A user-level systemd unit can start and stop an overlay with `steamvr.service`, restart it after accidental failure, and honor an intentional quit.
2. SteamVR can lose a dashboard overlay while its process remains alive. Frame Mic Tuner detects this with `FindOverlay` and recreates the dashboard surface, thumbnail, and panel texture.
3. Native Steam Frame dashboard interaction works with ordinary mouse-style OpenVR overlay events; no global controller action set is needed.
4. Polling and rendering can be heavily reduced when UI is hidden or stable. Frame Perf Overlay reports reducing wrist-mode CPU from about 1.97% to about 0.8% by doing this.
5. A second launch can signal the resident instance to show or toggle the UI, enabling a useful Dashboard `+` launcher experience.
6. `Compositor_FrameTiming` and Valve's built-in performance recording can help Sprint 10 distinguish PhoneCast load from game/compositor problems.

The plugins do **not** solve native H.264 hardware decoding or zero-copy decoder-to-compositor sharing. Their Vulkan path still copies CPU RGBA into a mapped staging buffer and waits for each upload fence. It is a strong correctness bridge, not the final high-performance video pipeline.

## 1. What the projects demonstrate

### Frame Perf Overlay

Frame Perf Overlay is a native AArch64 `VRApplication_Overlay` with:

- a persistent regular overlay that can be head- or controller-relative;
- a separate SteamVR dashboard settings overlay;
- a Vulkan `SetOverlayTexture` renderer;
- live scene/compositor frame statistics and Linux sensor data;
- configurable placement, scale, opacity, wrist fade, and update rate;
- user-level systemd lifecycle integration;
- a Dashboard `+` launcher and single-instance toggle behavior.

Its README includes an in-headset recording over Half-Life: Alyx and states that the software was tested on the author's Steam Frame. This is supporting evidence that a native regular overlay can coexist with an active VR scene. It is not a PhoneCast-controlled reproduction, so the stricter PhoneCast standalone-VR-scene validation remains open.

### Frame Mic Tuner

Frame Mic Tuner is a native AArch64 dashboard-only overlay with:

- laser/pointer-controlled dashboard UI;
- sliders, buttons, confirmation flows, and multiple tabs;
- Vulkan texture submission;
- automatic recovery if SteamVR drops its dashboard overlay;
- a background worker that keeps blocking system commands off the UI loop;
- work suppression while the panel is hidden;
- user-level systemd lifecycle integration and Dashboard `+` launch behavior.

Its microphone and WirePlumber modifications are outside PhoneCast's present scope and are firmware-sensitive. The relevant value is its robust dashboard lifecycle and interaction design, not its private knowledge of SteamOS audio internals.

## 2. Native Vulkan overlay rendering

Both repositories contain essentially the same `VulkanContext` and `OverlayTexture` implementation:

1. Initialize OpenVR as `VRApplication_Overlay`.
2. Query `GetVulkanInstanceExtensionsRequired`.
3. Create a Vulkan instance.
4. query `IVRSystem::GetOutputDevice(..., TextureType_Vulkan, instance)` and use the compositor's physical device;
5. query `GetVulkanDeviceExtensionsRequired` and create a logical device and graphics queue;
6. create persistent device-local RGBA images plus a persistently mapped, host-coherent staging buffer;
7. copy CPU RGBA to staging, issue `vkCmdCopyBufferToImage`, transition the image to `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`, and wait for a fence;
8. fill `VRVulkanTextureData_t` and call `SetOverlayTexture` using `TextureType_Vulkan` and `ColorSpace_Gamma`.

Frame Perf Overlay alternates between two Vulkan images. Its header records an observed reason for abandoning dynamic `SetOverlayRaw`: replacing raw data reportedly leaves the overlay without a texture for 15–40 ms and visibly flickers. This aligns with PhoneCast's independently observed Linux `SetOverlayRaw` flicker.

Relevant source:

- [Frame Perf Overlay `vk_texture.cpp`](https://github.com/sasaken1102r/frame-perf-overlay/blob/8baaf7e753ebd449c035c1d11a9830eba04d0201/src/vk_texture.cpp)
- [Frame Perf Overlay `vk_texture.h`](https://github.com/sasaken1102r/frame-perf-overlay/blob/8baaf7e753ebd449c035c1d11a9830eba04d0201/src/vk_texture.h)
- [Frame Perf Overlay OpenVR adapter](https://github.com/sasaken1102r/frame-perf-overlay/blob/8baaf7e753ebd449c035c1d11a9830eba04d0201/src/vr_overlay.cpp)

### PhoneCast implication

Use this as the reference for a **first native Frame renderer**, behind `IOverlayRenderer`:

```text
native decoder or temporary CPU RGBA source
        ↓
Steam Frame Vulkan renderer
        ↓
VRVulkanTextureData_t
        ↓
IVROverlay::SetOverlayTexture
```

This should replace repeated `SetOverlayRaw` updates on Linux. Keep the Windows D3D11 implementation unchanged and preserve platform separation.

Do not copy the implementation blindly:

- It filters out unavailable OpenVR-requested Vulkan extensions and continues. PhoneCast should treat a genuinely required unavailable extension as a clear initialization failure unless testing proves it optional.
- It performs a CPU `memcpy`, GPU transfer, and synchronous fence wait for every update. That may be acceptable for an initial correctness prototype but should be measured at phone resolution and 30 FPS.
- It does not provide hardware decode or direct decoder-surface import. Sprint 11 should keep decoder surfaces and Vulkan interop inside the Steam Frame platform layer so Core does not gain Vulkan handles.
- Synchronization and resource ownership must be validated under continuous video load, orientation changes, hide/show, and shutdown.

### Teardown order

The plugins use a careful shutdown sequence:

```text
HideOverlay where applicable
→ ClearOverlayTexture
→ DestroyOverlay
→ wait several compositor frames
→ VR_Shutdown
→ destroy Vulkan textures/device/instance
```

PhoneCast should adopt and physically test this ordering for its native Vulkan backend. It prevents freeing images while the compositor may still reference them.

## 3. Dashboard interaction and resilience

Both projects use the public OpenVR dashboard path:

- `CreateDashboardOverlay` for main and thumbnail handles;
- `SetOverlayInputMethod(..., VROverlayInputMethod_Mouse)`;
- `SetOverlayMouseScale` to map events into pixel coordinates;
- `PollNextOverlayEvent` for move, left-button down/up, focus leave, shown/hidden, and close events;
- Y-coordinate inversion because OpenVR mouse events use a bottom-left origin while their UI renderer uses a top-left origin.

They also enable `VROverlayFlags_EnableControlBarClose`, then read the flag back and log the result. A close event is treated as an intentional application quit rather than a SteamVR shutdown.

This independently supports PhoneCast's dashboard-first decision. Native dashboard interaction does not require overlay-global SteamVR Input bindings, wrist gestures, or claimed game controls.

### Dashboard self-repair

Frame Mic Tuner adds a particularly useful recovery loop:

1. Every three seconds, call `FindOverlay` with the expected key.
2. Compare the returned handle with the process's current handle.
3. If missing or replaced, clear/destroy only the process's old handle.
4. Recreate the dashboard overlay.
5. Resubmit the thumbnail and panel image.
6. Rate-limit duplicate failure logs and retry later if the key is temporarily in use.

The author documents SteamVR dashboard restarts as a real cause of a missing tab while the process remains running.

Relevant source:

- [Frame Mic Tuner `ensureOverlay`](https://github.com/sasaken1102r/frame-mic-tuner/blob/d1e8c830b00928ab94a44e1d5323613af95263c6/src/vr_overlay.cpp#L170-L219)
- [Frame Mic Tuner recovery loop](https://github.com/sasaken1102r/frame-mic-tuner/blob/d1e8c830b00928ab94a44e1d5323613af95263c6/src/main.cpp#L1338-L1365)

### PhoneCast implication

Add dashboard-handle health checks to the native backend and consider them for the Windows backend. Recovery must restore:

- dashboard main and thumbnail handles;
- input method and mouse scale;
- close-button flag;
- current rendered panel/thumbnail textures;
- active PhoneCast state without mutating portable settings.

Do not treat a valid OpenVR connection or a live process as proof that all overlay handles remain valid.

## 4. Placement and wrist behavior

Frame Perf Overlay provides another physically used controller-relative placement implementation. It:

- resolves left/right device IDs by controller role and caches the result for one second;
- uses `SetOverlayTransformTrackedDeviceRelative` for HMD and controller attachment;
- validates both HMD and controller tracking poses;
- hides immediately on tracking loss rather than leaving a panel floating at its last controller pose;
- computes the angle between the panel normal and direction to the HMD;
- applies a smooth fade through a configurable angular band;
- polls at about 30 Hz only while fading, and about 10 Hz while stable.

Relevant source:

- [placement calculations](https://github.com/sasaken1102r/frame-perf-overlay/blob/8baaf7e753ebd449c035c1d11a9830eba04d0201/src/placement.cpp)
- [placement update](https://github.com/sasaken1102r/frame-perf-overlay/blob/8baaf7e753ebd449c035c1d11a9830eba04d0201/src/vr_overlay.cpp#L313-L390)
- [placement tests](https://github.com/sasaken1102r/frame-perf-overlay/blob/8baaf7e753ebd449c035c1d11a9830eba04d0201/tests/wrist_clock_test.cpp)

### PhoneCast implication

PhoneCast's configurable controller placement is more general and should remain the owning model. Two ideas are worth optional follow-up:

- an opt-in "show when facing me" wrist fade for Glance mode;
- explicit policy selection for tracking loss: retain-last-pose for stable expanded placement versus hide-immediately for glance/wrist-watch presentation.

These are UX options, not reasons to replace the existing portable placement model. Physical ergonomic testing is still required.

## 5. Lifecycle, autostart, and launch behavior

Before initializing as an overlay, both projects briefly try `VRApplication_Background` to distinguish "SteamVR is not running" from a real initialization failure without starting SteamVR themselves. They then retry on a slow interval until the runtime appears. This is appropriate for a service coupled to SteamVR and avoids the overlay unexpectedly launching the runtime.

Both projects install a user-level systemd service with:

```ini
After=steamvr.service
PartOf=steamvr.service
Restart=always
RestartSec=5
WantedBy=steamvr.service
```

They use a special exit code for intentional dashboard Quit/Close and list it in `RestartPreventExitStatus`, so accidental exits restart but a user-requested exit stays stopped until the next SteamVR start. `PartOf=steamvr.service` stops the plugin with SteamVR.

This is a concrete alternative or complement to OpenVR `SetApplicationAutoLaunch` for Sprint 12. It also avoids writing system files: installation is under the user's home directory.

Both projects install a `.desktop` launcher and multiple icon sizes under `~/.local/share`. Frame Perf Overlay states that only installing a 256 px icon was insufficient for discovery, so it installs 48, 128, and 256 px variants. Absolute `Exec` paths are used because Steam-launched processes may not have `~/.local/bin` on `PATH`.

Their native builds bundle the OpenVR 2.15.6 header but dynamically link the SteamVR runtime's `/opt/steamvr/bin/linuxarm64/libopenvr_api.so` with an RPATH to that directory. This demonstrates another workable packaging choice, but PhoneCast currently gets reproducibility from its pinned, statically linked OpenVR client. Do not change that choice without testing ABI/runtime update behavior and deployment size.

Both projects enforce a single resident instance with `flock`. A second manual launch signals the resident process:

- Perf Overlay toggles the regular panel.
- Mic Tuner opens its dashboard panel with `ShowDashboard`.

They distinguish a systemd restart from a manual second launch so `Restart=always` cannot repeatedly toggle/open the UI.

Relevant source:

- [Perf Overlay systemd unit](https://github.com/sasaken1102r/frame-perf-overlay/blob/8baaf7e753ebd449c035c1d11a9830eba04d0201/contrib/frame-perf-overlay.service)
- [Perf Overlay installer](https://github.com/sasaken1102r/frame-perf-overlay/blob/8baaf7e753ebd449c035c1d11a9830eba04d0201/install.sh)
- [Mic Tuner systemd unit](https://github.com/sasaken1102r/frame-mic-tuner/blob/d1e8c830b00928ab94a44e1d5323613af95263c6/contrib/frame-mic-tuner.service)

### PhoneCast implication

For native packaging, test this exact lifecycle pattern on Steam Frame:

1. install a user service coupled to `steamvr.service`;
2. retain the registered OpenVR application identity/manifest until native VR-scene coexistence proves it unnecessary;
3. install a Dashboard `+` launcher that signals an existing receiver rather than starting a duplicate;
4. make intentional Quit distinct from crashes and runtime shutdown;
5. verify executable mode, absolute paths, icons, logs, sleep/wake, SteamVR restart, and OS update survival.

PhoneCast should not choose between systemd and OpenVR autolaunch only from source inspection. Test both on hardware and use the simplest reliable combination.

## 6. Low-overhead update loops

The plugins avoid constant full-rate UI work:

- render dashboard panels only when visible and dirty;
- perform expensive state reads only while the relevant panel is visible;
- use slower polling when hidden or stable and faster polling during pointer activity, dragging, or animation;
- coalesce pending slider writes so only the newest value is processed;
- move blocking commands and reads to a worker thread;
- avoid catch-up bursts after a delayed periodic update;
- cache controller-role lookup and slow-changing battery/autostart state.

Frame Perf Overlay's changelog reports wrist-mode CPU falling from about 1.97% to about 0.8% after adaptive polling. This is an author measurement, not independently verified.

### PhoneCast implication

Apply the same scheduling principles without breaking instant reveal:

- continue receiving/decoding while Hidden as currently designed, but stop unnecessary overlay/UI redraws and slow noncritical diagnostics;
- render dashboard/settings textures only when their visible state changes;
- use dirty flags for menu state and pointer hover;
- coalesce high-frequency adjustment events;
- never "catch up" stale video or UI ticks after a stall;
- expose a future battery-saving hidden-state policy only after measuring reveal latency.

## 7. Performance diagnostics useful to Sprint 10

Frame Perf Overlay reads recent `Compositor_FrameTiming` records to derive:

- displayed application FPS;
- GPU and CPU frame time;
- reprojection percentage;
- dropped frames;
- compositor throttling level;
- display refresh rate.

The author reports that FPS and reprojection match SteamVR's own frame timing records. GPU utilization, some power channels, and streaming-specific interpretations are explicitly labeled estimates.

Valve also documents a built-in Frame performance overlay and a CSV performance recorder under `/home/steamos/.local/share/Steam/logs`. The CSV includes FPS, peak CPU/GPU time, reprojected frames, GPU clock, power, refresh rate, throttling, and submitted resolution.

Sources:

- [Frame Perf Overlay frame-stat implementation](https://github.com/sasaken1102r/frame-perf-overlay/blob/8baaf7e753ebd449c035c1d11a9830eba04d0201/src/vr_overlay.cpp#L534-L648)
- [Valve: Steam Frame debugging and performance recording](https://partner.steamgames.com/doc/steamhardware/steamframe/debugging)

### PhoneCast implication

Sprint 10 should combine two classes of measurements:

1. **PhoneCast-owned:** receive FPS, queue age/depth, decode time, conversion/upload time, submitted frames, dropped video frames, memory, CPU, and network bitrate.
2. **Runtime-owned:** scene FPS, frame times, reprojection, throttling, and power from Valve's recorder or OpenVR timing APIs.

This allows a test to answer both "is PhoneCast's stream healthy?" and "did PhoneCast harm the VR game?" Avoid presenting estimated GPU utilization or undocumented sensor meanings as exact values.

## 8. UI and safety patterns

Useful UI practices visible in the projects include:

- submit a loading image before the panel is first selected;
- render screenshots without SteamVR for UI review;
- test hit geometry and configuration parsing without VR hardware;
- use press/release/focus-leave handling so sliders and pressed states cannot remain stuck;
- require a second confirmation for Quit and Update;
- stop privacy-sensitive work immediately when its UI closes;
- save configuration through a temporary file followed by rename;
- clamp values, retain defaults for missing keys, and log unknown or malformed input.

Frame Mic Tuner also demonstrates an important boundary: user-level installation does not automatically make platform modification safe. Its WirePlumber override can break after a SteamOS update and potentially disrupt all headset audio. PhoneCast should avoid firmware-coupled overrides unless a feature explicitly requires them, documents rollback, and has hardware validation.

For the future optional phone-audio sprint, the general lessons are useful: do not restart headset audio services during play, clearly stop background capture/playback, keep temporary media in memory where practical, and expose explicit user control. The microphone-specific implementation itself should not be imported.

## 9. Recommended PhoneCast actions

### High priority for Sprint 11

1. Prototype a `SteamFrameVulkanOverlayRenderer` from the plugins' Vulkan/OpenVR path.
2. Preserve the platform boundary; do not expose Vulkan objects through Core.
3. Replace Linux dynamic `SetOverlayRaw` with persistent `SetOverlayTexture` resources.
4. Benchmark the temporary CPU-RGBA staging path at real portrait and landscape stream sizes.
5. Add correct clear/destroy/wait/shutdown/Vulkan-destroy ordering.
6. Add dashboard overlay health detection and recreation.
7. Physically validate native regular-overlay coexistence over a standalone VR scene.

### High priority for Sprint 12

1. Test a user-level service coupled to `steamvr.service`.
2. Test Dashboard `+` launch with single-instance show behavior.
3. Compare systemd lifecycle with `SetApplicationAutoLaunch`/`LaunchDashboardOverlay`.
4. Verify SteamVR restart, dashboard restart, game switching, sleep/wake, intentional quit, crash restart, and OS update survival.
5. Package absolute launcher paths, executable bits, and 48/128/256 px icons.

### Performance pass

1. Correlate PhoneCast telemetry with Valve performance-recording CSV data.
2. Add scene FPS/reprojection/throttling only as an optional diagnostics layer.
3. Use dirty rendering and adaptive polling for dashboards and settings.
4. Measure before adopting synchronous per-frame Vulkan fence waits as the final video design.

### Optional UX follow-up

1. Evaluate wrist-facing fade for Glance only.
2. Make tracking-loss behavior state-specific rather than globally changing current placement behavior.
3. Keep dashboard controls primary; do not reactivate broad overlay-global bindings.

## 10. What remains unknown

These repositories do not establish:

- PhoneCast's native H.264 decoder choice;
- hardware decode availability and API stability on Steam Frame;
- zero-copy import from decoder output to Vulkan/OpenVR;
- sustained 30 FPS performance at PhoneCast resolutions;
- the latency impact of per-frame staging and fence waits;
- PhoneCast's native dashboard and regular-overlay behavior after sleep/wake;
- whether PhoneCast should use systemd, OpenVR autolaunch, or both;
- whether every standalone VR title permits the same overlay coexistence observed by the Perf Overlay author.

These remain hardware test items, not completed design claims.

## Conclusion

The two plugins materially reduce risk for PhoneCast's native backend. Their strongest contribution is a public, native ARM64 Vulkan `SetOverlayTexture` implementation that matches PhoneCast's compositor/GPU-selection needs and offers a practical way past `SetOverlayRaw` flicker. Their dashboard recovery, systemd lifecycle, single-instance launcher, adaptive polling, and compositor diagnostics are also directly reusable as design patterns.

The recommended direction is not to copy either application wholesale. Reuse the proven native patterns behind PhoneCast's existing Core/VR/platform boundaries, keep the current dashboard-first interaction model, and treat hardware decoding plus decoder-to-Vulkan sharing as a separate measured problem.
