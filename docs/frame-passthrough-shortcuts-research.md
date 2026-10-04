# Frame Passthrough Shortcuts research

Research date: 2026-10-03  
Source reviewed: [`KominoVR/frame-passthrough-shortcuts`](https://github.com/KominoVR/frame-passthrough-shortcuts) at commit [`032841d`](https://github.com/KominoVR/frame-passthrough-shortcuts/tree/032841dcde61059b25d33a487bf202a30fbf58f1)  
Scope: SteamVR Input behavior, dashboard focus, overlay-global shortcuts, native Steam Frame startup, and lessons for PhoneCast

## Executive summary

The plugin confirms that a native Linux ARM64 `VRApplication_Overlay` can receive controller shortcuts while another application owns scene focus. It does so through an explicit SteamVR Input action set with overlay-global priority. It does **not** create or render an overlay surface.

The README warning that shortcuts work only with the dashboard closed is significant. The source explicitly records that even the highest public overlay-global priority cannot override SteamVR dashboard input focus. This gives PhoneCast a clearer input model:

```text
Dashboard open
    → dashboard owns controller interaction
    → use PhoneCast dashboard pointer controls

Dashboard closed
    → an overlay-global action set may receive configured shortcuts
    → only if overlay input overrides/runtime policy permits it
```

The most important new PhoneCast finding was a safety issue in the dormant-input implementation: disabling the wrist and thumbstick menu actions did **not** initially stop PhoneCast from activating its broad overlay-global action set every frame. That set binds thumbstick clicks and axes, grips, triggers, and calibration buttons. OpenVR documents that a higher-priority set disables lower-priority bindings using the same source.

**Resolution:** supported dashboard-only operation no longer initializes or submits that action set. PhoneCast therefore does not call `UpdateActionState` for the broad global bindings during ordinary use. If shortcuts are revisited later, use a separate, narrow, opt-in hotkey action set, activate it only when appropriate, and bind only the specific button gesture required.

---

## 1. How the plugin works

### Application identity and lifecycle

The plugin:

1. initializes as `VRApplication_Overlay`;
2. registers an application manifest with `is_dashboard_overlay: true`;
3. includes both `binary_path_linux` and Steam Frame's required `binary_path_linux_arm`;
4. identifies its running PID with its registered application key;
5. loads an explicit SteamVR Input action manifest;
6. creates **no** overlay surface;
7. enables SteamVR autostart with `SetApplicationAutoLaunch` during installation;
8. uses `LaunchDashboardOverlay` to start the registered overlay application if it is not already running.

Relevant source: [`src/main.cpp` lines 141–165](https://github.com/KominoVR/frame-passthrough-shortcuts/blob/032841dcde61059b25d33a487bf202a30fbf58f1/src/main.cpp#L141-L165) and [lines 625–634](https://github.com/KominoVR/frame-passthrough-shortcuts/blob/032841dcde61059b25d33a487bf202a30fbf58f1/src/main.cpp#L625-L634).

This independently reinforces PhoneCast's existing findings about `binary_path_linux_arm`, application identification, and overlay classification. It also provides a concrete native-Frame example for future Sprint 12 autostart/lifecycle work.

### SteamVR Input action set

The plugin declares six optional Boolean actions in one `leftright` action set. Its default Frame Controller binding maps either thumbstick as a `button` source:

- double press, within 0.25 seconds: toggle passthrough style;
- one-second long press: toggle RGB/monochrome source.

SteamVR's binding layer recognizes `double` and `long`; the application does not implement those timers itself. See [`assets/bindings/frame_controller.json`](https://github.com/KominoVR/frame-passthrough-shortcuts/blob/032841dcde61059b25d33a487bf202a30fbf58f1/assets/bindings/frame_controller.json).

At runtime it:

- resolves the action-set and action handles;
- submits the action set on every polling iteration;
- uses `k_nActionSetOverlayGlobalPriorityMax`;
- reads `InputDigitalActionData_t`;
- requires `bActive && bState && bChanged` before firing;
- adds a second application-level debounce gate;
- polls every 20 ms.

Relevant source: [`src/main.cpp` lines 198–237](https://github.com/KominoVR/frame-passthrough-shortcuts/blob/032841dcde61059b25d33a487bf202a30fbf58f1/src/main.cpp#L198-L237) and [lines 278–330](https://github.com/KominoVR/frame-passthrough-shortcuts/blob/032841dcde61059b25d33a487bf202a30fbf58f1/src/main.cpp#L278-L330).

### Diagnostics

The plugin records information PhoneCast currently lacks:

- `VREvent_Input_BindingLoadFailed`;
- each action's `bActive` state;
- `IsDashboardVisible()`;
- selected action-set priority;
- action update/read errors;
- a warning if no binding becomes active within 30 seconds.

This is materially better than treating a successful action-manifest load or successful `UpdateActionState` as proof that controller actions are deliverable.

---

## 2. Why shortcuts require the dashboard to be closed

This is not an arbitrary limitation in the plugin.

The plugin uses `k_nActionSetOverlayGlobalPriorityMax`, then states in source:

> even the highest public priority cannot override dashboard input focus

It also publishes `IsDashboardVisible()` in runtime diagnostics. See [`src/main.cpp` lines 231–237](https://github.com/KominoVR/frame-passthrough-shortcuts/blob/032841dcde61059b25d33a487bf202a30fbf58f1/src/main.cpp#L231-L237) and [lines 315–316](https://github.com/KominoVR/frame-passthrough-shortcuts/blob/032841dcde61059b25d33a487bf202a30fbf58f1/src/main.cpp#L315-L316).

The observed hierarchy is therefore:

1. SteamVR dashboard/system UI input focus;
2. overlay-global action sets, when the dashboard is closed and the runtime permits overlay overrides;
3. ordinary scene-application action sets.

This is useful rather than problematic for PhoneCast:

- while the dashboard is open, PhoneCast already has reliable spatial pointer input on its dashboard panel;
- a background shortcut is only useful while the dashboard is closed;
- PhoneCast should never try to make a global shortcut fire while the user is selecting dashboard controls.

The plugin does not explicitly skip `UpdateActionState` while the dashboard is open. Instead, SteamVR makes the actions unavailable. For PhoneCast, intentionally deactivating or ignoring a future hotkey action set while `IsDashboardVisible()` is true would make the policy clearer and reduce accidental behavior.

---

## 3. Comparison with PhoneCast

| Area | Frame Passthrough Shortcuts | Current PhoneCast | Lesson |
|---|---|---|---|
| Runtime | Native Linux ARM64 Steam Frame | Windows/VRLink today; ARM64 receiver not yet complete | Native success does not prove identical VRLink behavior |
| App type | `VRApplication_Overlay` | `VRApplication_Overlay` | Same correct application type |
| Overlay surface | None | Phone, settings, and dashboard overlays | A rendered overlay is not required for global actions |
| Identity | Registers manifest, verifies/identifies PID | Registers manifest and identifies PID | Broadly aligned |
| Action priority | `k_nActionSetOverlayGlobalPriorityMax` | Global action set disabled in supported operation; dormant code uses `Min` | Do not reactivate the broad set; max is only a future narrow-hotkey test variable |
| Dashboard behavior | Actions blocked while dashboard is open | Dashboard controls work; earlier shortcut delivery failed | Retest, if ever needed, with dashboard explicitly closed |
| Binding scope | Thumbstick button gesture only | Thumbstick click/axis, grip, trigger, View/Menu | PhoneCast's set is much more invasive |
| Gesture timing | SteamVR binding modes `double` and `long` | Application-side hold timer | Prefer binding-layer gestures for user-remappable shortcuts |
| Input diagnostics | Per-action `bActive`, binding-load failures, dashboard state | Mostly initialization/update errors | Add state diagnostics before any future shortcut experiment |
| Startup | Verified autolaunch registration flow in code | Autostart deferred | Useful Sprint 12 reference |

### Priority difference

OpenVR defines the entire range from `k_nActionSetOverlayGlobalPriorityMin` to `k_nActionSetOverlayGlobalPriorityMax` as overlay-global. It also says bindings in a higher-priority active set disable bindings in lower-priority sets that use the same source. See Valve's [`openvr.h`](https://github.com/ValveSoftware/openvr/blob/0924064316de3effbcd1acf1e309182a2deb1c05/headers/openvr.h#L5286-L5327).

The plugin's use of `Max` is worth preserving as a test variable, but it does not prove PhoneCast's use of `Min` caused the failed test. Other variables include:

- dashboard visibility;
- whether experimental overlay input overrides were enabled;
- Windows/VRLink versus native Steam Frame;
- binding activation and controller-type resolution;
- PhoneCast's broad `joystick` binding versus the plugin's narrow `button` binding.

---

## 4. PhoneCast input-safety correction

PhoneCast now has two independent compile-time policy gates:

```cpp
constexpr bool kEnableExperimentalGestureControls = false;
constexpr bool kEnableExperimentalOverlayGlobalInput = false;
```

The first prevents the wrist recognizer and thumbstick short/long-press handlers from opening UI. The second prevents `InitializeControllerInput()` from running in the supported dashboard-only configuration. Consequently, `PollExplicitInput()` cannot submit `/actions/phonecast` through `UpdateActionState`, and the broad global bindings do not claim:

- thumbstick clicks or axes;
- grips or triggers;
- left View or right Menu.

Legacy state polling remains as a non-global compatibility path, but ordinary UI is provided by dashboard overlay pointer events. Users do not need SteamVR's experimental overlay input overrides.

If controller calibration needs explicit action input later, enter calibration from the dashboard and activate a separate, narrowly scoped action set only for that mode. Do not re-enable the broad set.

---

## 5. Better design if PhoneCast revisits hotkeys

Use two action sets rather than one broad set:

```text
/actions/phonecast_hotkeys
    one or two optional Boolean shortcuts
    overlay-global only while dashboard is closed

/actions/phonecast_calibration
    calibration-specific controls
    active only after explicit entry from the dashboard
```

Recommended rules:

1. Make global hotkeys opt-in and user-remappable.
2. Bind the smallest possible source surface—prefer a click gesture, not thumbstick position, grips, and triggers together.
3. Use SteamVR binding modes such as `double` or `long` instead of maintaining timing logic in PhoneCast.
4. Use `k_nActionSetOverlayGlobalPriorityMax` only for a controlled experiment; the whole range is experimental and can suppress game bindings.
5. Never expect the shortcut to fire while the dashboard is visible.
6. Deactivate the global set while `IsDashboardVisible()` is true.
7. Log `bActive`, `bState`, `bChanged`, active origin, binding-load failures, dashboard visibility, runtime/backend, and selected priority.
8. Warn clearly if no action becomes active.
9. Test native Steam Frame and Windows/VRLink separately.
10. Verify that unclaimed game controls continue to work and that no input remains stuck after action-set deactivation.

A minimal future experiment should compare:

| Variable | Cases |
|---|---|
| Backend | native Frame; Windows/VRLink |
| Dashboard | closed; open |
| Priority | normal; overlay-global min; overlay-global max |
| Binding | thumbstick `button`/double; thumbstick `button`/long |
| Override setting | disabled; enabled |
| Scene | no scene; active VR game |

This experiment is optional. The approved PhoneCast dashboard already supplies a reliable interaction path.

---

## 6. Other reusable knowledge

### Autostart

The plugin provides a practical native lifecycle pattern:

```text
register manifest
→ SetApplicationAutoLaunch(app_key, true)
→ verify GetApplicationAutoLaunch(app_key)
→ LaunchDashboardOverlay(app_key) if not running
```

This is relevant to PhoneCast Sprint 12. It must still be tested with PhoneCast's actual native process, dashboard tab, networking, sleep/wake, and application transitions.

### Binding UI

The plugin exposes `OpenBindingUI` for its action set. If PhoneCast ever ships optional shortcuts, users should be able to inspect and change them through SteamVR rather than editing JSON.

### Installation robustness

The plugin uses absolute generated paths and stores launch configuration outside manifest arguments because it reports that Frame's manifest argument parser preserves quote characters literally. This is useful packaging knowledge for paths containing spaces. The repository's first public issue also reports that executable permission loss in one FrameDrop install prevented launch until `chmod +x`; native PhoneCast packaging should verify executable bits.

### Private camera APIs are not reusable guidance

The plugin's passthrough camera-source implementation uses the private `IVRCameraPassthroughInternal_001` interface and fixed vtable slots verified against one firmware build. This is explicitly fragile and unrelated to PhoneCast's overlay/input architecture. PhoneCast should not copy that technique unless a future feature explicitly requires it and accepts firmware coupling.

---

## 7. Evidence classification

### Documented by Valve

- SteamVR Input action manifests and bindings map device inputs to application actions.
- Overlay applications may use the experimental overlay-global priority range.
- A higher-priority active action set disables lower-priority bindings using the same source.
- `IsDashboardVisible`, application autolaunch, binding UI, and dashboard-overlay launch APIs exist.

### Demonstrated by the plugin's source and README

- Native ARM64 overlay application registration and autostart are implemented.
- The application runs without creating an overlay surface.
- Frame Controller double/long thumbstick bindings are declared using SteamVR's binding modes.
- The application uses maximum overlay-global priority.
- The author reports that shortcuts work only while the dashboard is closed.
- Runtime diagnostics correlate action activity with dashboard visibility.

### Physically tested in PhoneCast

- The PhoneCast dashboard icon, panel, and controller-laser controls work on Windows/VRLink.
- PhoneCast's earlier global controller actions did not deliver commands in the tested setup.
- The earlier test did not collect enough per-action state to isolate dashboard focus, binding activation, priority, override settings, or backend differences.

### Still unknown

- Whether PhoneCast hotkeys would work with the dashboard closed at maximum priority on Windows/VRLink.
- Whether PhoneCast's bindings work natively on Steam Frame.
- Whether experimental overlay input overrides were enabled and effective during every prior PhoneCast retest.
- Whether native PhoneCast autostart will survive sleep/wake and application transitions.

---

## 8. Decision for PhoneCast

No change to the current product direction is required:

- keep the OpenVR dashboard as the supported control surface;
- keep wrist and thumbstick menu gestures disabled;
- keep ordinary activation of the broad overlay-global action set disabled;
- use the plugin's diagnostics and narrow binding strategy only if optional dashboard-closed hotkeys are revisited;
- reuse the autostart pattern as a reference during standalone lifecycle work.

The plugin shows that dashboard-closed overlay hotkeys are feasible on native Steam Frame. It does not show that PhoneCast needs them, and it reinforces why they should be narrow, optional, and subordinate to dashboard focus.

---

## Sources

1. KominoVR, **Frame Passthrough Shortcuts repository**, reviewed at commit `032841dcde61059b25d33a487bf202a30fbf58f1`  
   https://github.com/KominoVR/frame-passthrough-shortcuts/tree/032841dcde61059b25d33a487bf202a30fbf58f1
2. Plugin runtime and registration implementation  
   https://github.com/KominoVR/frame-passthrough-shortcuts/blob/032841dcde61059b25d33a487bf202a30fbf58f1/src/main.cpp
3. Plugin Frame Controller binding  
   https://github.com/KominoVR/frame-passthrough-shortcuts/blob/032841dcde61059b25d33a487bf202a30fbf58f1/assets/bindings/frame_controller.json
4. Plugin action manifest  
   https://github.com/KominoVR/frame-passthrough-shortcuts/blob/032841dcde61059b25d33a487bf202a30fbf58f1/assets/actions.json
5. Valve OpenVR, **SteamVR Input**  
   https://github.com/ValveSoftware/openvr/wiki/SteamVR-Input
6. Valve OpenVR, **Action manifest**  
   https://github.com/ValveSoftware/openvr/wiki/Action-manifest
7. Valve OpenVR SDK header, action-set priority contract  
   https://github.com/ValveSoftware/openvr/blob/0924064316de3effbcd1acf1e309182a2deb1c05/headers/openvr.h#L5286-L5327
8. Plugin issue #1, executable permission failure during FrameDrop installation  
   https://github.com/KominoVR/frame-passthrough-shortcuts/issues/1
