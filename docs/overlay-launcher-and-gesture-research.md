# Overlay launch controls and gesture-control research

Research date: 2026-10-03  
Scope: PhoneCast launcher and gesture-control research  
Implementation status: **OpenVR dashboard implemented and physically approved on Windows/VRLink 2026-10-03; gesture entry disabled; native ARM64 lifecycle pending**

## Executive recommendation

PhoneCast should use a layered control strategy rather than treating the wrist gesture as the only way into the UI:

1. **Primary reliable entry point:** add a supported OpenVR **dashboard overlay tab** with a PhoneCast icon and large Show/Hide, Glance, Pin, Settings, and placement buttons.
2. **Reliable in-scene interaction:** retain controller-laser/mouse interaction on visible PhoneCast overlays. Consider a small contextual toolbar/handle, but do not leave a permanent scene overlay enabled by default.
3. **Convenience path:** improve the wrist gesture into a filtered, hysteretic, multi-stage recognizer. It should never be the sole recovery path.
4. **Optional expert path:** keep SteamVR action bindings as opt-in shortcuts only. Overlay-global action priority is experimental, can consume a game's bound controls, and has not yet delivered commands in the physical PhoneCast test.
5. **Do not target Steam Frame Quick Access injection:** no documented public API was found for placing a third-party button inside Valve's native Quick Access/Quick Settings area. The supported neighboring extension point is a dashboard overlay tab.

The dashboard-tab prototype is implemented, and its icon, panel, and laser controls are physically approved on Windows/VRLink. It is now the supported in-headset entry point. Extended lifecycle testing will accumulate during normal use; native ARM64 behavior remains a separate validation item. Wrist-pose and thumbstick long-press menu gestures are disabled for now.

---

## 1. Current PhoneCast state

PhoneCast currently:

- initializes OpenVR as an overlay application;
- registers `com.phonecastvr.receiver` with `is_dashboard_overlay: true`;
- creates regular scene overlays with `CreateOverlay` for the phone, control grid, gesture progress, and settings;
- creates a best-effort dashboard main/thumbnail pair with `CreateDashboardOverlay` and a static PhoneCast launcher icon;
- routes dashboard Show/Hide, Glance, Pin, Settings, and placement clicks through the same portable actions as the scene control grid;
- enables mouse-style input and `VROverlayFlags_MakeOverlaysInteractiveIfVisible` for scene-overlay laser interaction;
- ships explicit Frame Controller action bindings;
- requests the experimental overlay-global action-set priority;
- has physically loaded those bindings, but has not received controller button commands in the tested Steam Frame/VRLink setup;
- retains the pose-only wrist recognizer as dormant experimental code, disabled at runtime.

The current wrist detector is intentionally small, but primitive:

- it assumes controller local `+Y` is the control-face normal;
- it accepts a broad facing threshold of dot product `>= 0.62` (about a 52-degree cone);
- it accepts controller-to-head distances from 0.18 to 0.90 m and a broad vertical range;
- it reduces each hand to a binary `facing/not facing` value;
- both hands must first be outside the pose to arm it;
- exactly one hand must then remain inside the pose continuously for three seconds;
- it has no filtering, hysteresis, short tracking-loss grace period, velocity/raise sequence, hand lock, confidence score, or cooldown beyond re-arming.

This explains both poor intentionality and poor responsiveness: a broad static pose can be entered accidentally, while a three-second uninterrupted dwell makes intended use slow and fragile.

---

## 2. What Valve publicly supports

### 2.1 Dashboard overlay tabs are the supported launcher surface

OpenVR documents `IVROverlay::CreateDashboardOverlay` as creating:

- a **main handle** for the application's dashboard UI; and
- a **thumbnail handle** for the icon/tab shown when the user opens the VR Dashboard with the controller system button.

`IVROverlay::ShowDashboard(key)` opens the dashboard with a named dashboard overlay selected. Dashboard overlays are always mouse-input overlays, and tracked-controller pointing is delivered as mouse-style overlay events.

This is a close match for the requested launcher/control surface. It is not an injected Quick Access button; it is a first-class dashboard tab/icon managed by SteamVR.

Valve also documents SteamVR overlay applications as the mechanism for adding dashboard tabs or functionality while other VR applications run. Steam-distributed products use the “Launch SteamVR Overlay” launch option.

### 2.2 Quick Access is not a documented extension point

SteamVR 2.17's Quick Access area is described as Valve-owned system UI for notifications, volume, brightness, batteries, and related system controls. Research found no Valve/Steamworks/OpenVR API for adding arbitrary third-party buttons or pages to that native area.

Therefore:

- **supported:** a PhoneCast dashboard overlay icon/tab;
- **not publicly documented:** a PhoneCast button embedded directly in native Quick Access/Quick Settings;
- **not recommended:** Steam client UI patching, injection, DOM manipulation, or depending on undocumented internal interfaces.

Unsupported UI modification would be brittle across SteamVR updates, difficult to ship safely, and especially inappropriate for the long-term native ARM64 target.

### 2.3 A dashboard tab and the scene phone overlay can coexist

A single overlay application may own multiple overlay handles. The recommended model is:

```text
PhoneCast process
├── regular scene overlay: live phone image
├── regular scene overlay: optional contextual controls/settings
└── dashboard overlay
    ├── dashboard main panel
    └── dashboard thumbnail/icon
```

The dashboard panel should control the existing portable `GlanceController`, `OverlayController`, and settings model. It should not introduce a second source of truth.

### 2.4 Dashboard presence versus process launch

A dashboard icon exists only while the overlay application is registered/running and has created its dashboard overlay. OpenVR exposes `IVRApplications::SetApplicationAutoLaunch`, documented for dashboard-overlay applications, but startup behavior still needs direct validation on Steam Frame.

PhoneCast should separate two questions:

1. Can a running native/PC PhoneCast process reliably expose and operate its dashboard tab?
2. Can PhoneCast be made to start automatically/reliably enough that the tab is present after SteamVR starts, wakes, or changes applications?

Do not treat manifest registration alone as proof of either behavior.

### 2.5 Button-action limitations

SteamVR Input's supported action-manifest flow is sound for ordinary applications: declare actions, provide device bindings, call `UpdateActionState`, and read action data. Frame Controller's OpenVR type is `frame_controller`, and Valve documents click/touch/value inputs, grip and aim poses, haptics, and skeletal input.

For an overlay to pre-empt a scene application's controls, OpenVR exposes an **experimental overlay-global priority range**. Valve's header notes that the user must enable experimental overlay input overrides. Higher-priority actions suppress lower-priority actions bound to the same physical source.

Consequences for PhoneCast:

- a global thumbstick-click binding can steal that click from the game;
- enabling the feature is a developer/user setting, not a dependable default product path;
- loaded bindings do not prove active cross-application delivery, as the current physical result demonstrates;
- binding every trigger, grip, axis, and menu control globally is too invasive;
- dashboard pointer clicks are preferable while the dashboard is open because the user has explicitly entered system UI.

---

## 3. Control-surface options

| Option | Reliability | Game conflict | Hidden-overlay recovery | Portability | Recommendation |
|---|---:|---:|---:|---:|---|
| OpenVR dashboard tab/icon | High by documented design; hardware test required | Low | Yes, if process/tab is present | OpenVR backend; portable commands beneath it | **Primary** |
| Dashboard panel buttons via pointer/mouse events | High while dashboard is active | Low | Yes | Backend-specific rendering, portable actions | **Primary** |
| Visible scene-overlay buttons/toolbar | Likely good while visible | Low if click is spatially targeted | No if all PhoneCast overlays are hidden | OpenVR today; reusable UI model | Secondary |
| Tiny persistent summon handle | Likely good if compositor permits it | Low | Yes | Backend implementation required | Optional, off by default |
| Wrist/controller pose gesture | Variable; must be tuned physically | None | Yes | Portable recognizer over backend pose features | Convenience only |
| Overlay-global action bindings | Currently unconfirmed; experimental | Potentially high | Yes | OpenVR-specific | Opt-in expert feature |
| Native Quick Access injection | No documented API | Unknown | Potentially | Brittle/internal | **Reject** |
| Keyboard shortcuts | Reliable development fallback | None in headset | Only with PC access | Windows-hosted only | Development fallback |

### Recommended dashboard panel

Keep the first panel deliberately small:

```text
┌──────────────────────────┐
│ PhoneCast                │
│ Connected • 590×1280     │
│                          │
│ [ Show / Hide ] [Glance] │
│ [ Pin ]         [Settings]│
│                          │
│ Placement                │
│ [Head] [World] [Left] [Right]
│                          │
│ Receiver: Connected      │
└──────────────────────────┘
```

Requirements:

- large laser targets;
- no hover-only destructive action;
- visible current state;
- one-click Show/Hide recovery;
- no stream contents or sensitive notification text in the dashboard thumbnail;
- no duplicate persistence logic;
- input processed only while the tab is active;
- clean behavior if the phone is disconnected;
- dashboard closure must release interaction immediately.

The thumbnail should be a static, recognizable PhoneCast icon. Do not update it per video frame.

### Optional contextual scene toolbar

When the phone is visible and pointed at, a small edge toolbar could expose Hide, Pin, Move, and Settings. Prefer one of these behaviors after physical comparison:

- appear only while the pointer hovers the phone;
- appear after clicking a small non-destructive handle;
- appear only while a menu/settings mode is active.

Avoid an always-visible toolbar that reduces usable phone area or distracts during play. This path does not solve recovery when PhoneCast is completely hidden, so it cannot replace the dashboard tab.

---

## 4. Gesture-control research

### 4.1 Available signals on Steam Frame

Valve documents Frame Controllers as providing:

- 6-DOF controller tracking;
- click, touch, and analog values for controls;
- grip and aim poses;
- capacitive finger tracking;
- OpenVR Skeletal Input support;
- haptics.

Valve's public Frame documentation does **not** currently establish a bare-hand optical tracking/gesture API equivalent to `XR_EXT_hand_tracking`. Controller pose and optional controller-derived skeletal/finger data are the practical inputs to investigate.

OpenVR skeletal actions can provide bone transforms, a compact skeletal summary, and a reported tracking level. Because skeletal data uses the action system, PhoneCast must first test whether those actions remain active for an overlay while another application owns scene focus. Finger data is an enhancement, not a safe dependency yet.

### 4.2 Better recognizer design

Replace the current binary static-pose check with a portable feature observation and explicit finite-state machine:

```text
Unavailable
   ↓ tracking valid
Idle
   ↓ deliberate away/rest condition
Armed
   ↓ selected hand raises/turns toward user
Candidate
   ↓ stable intentional pose for dwell
Triggered
   ↓ pose exits + cooldown
Idle/Armed
```

#### A. Validate coordinate assumptions first

Before tuning thresholds, record controller bases while the user performs:

- controller held naturally at side;
- control face toward headset;
- control face away;
- wrist-up checking pose;
- controller aim ray at the menu.

Confirm whether local `+Y` is consistently the physical control-face normal for native Frame and VRLink paths. If not, use a resolved pose action or a per-controller profile transform rather than a hard-coded matrix column.

#### B. Compute continuous features

The OpenVR backend should produce continuous, backend-neutral features rather than `leftFacing/rightFacing` booleans:

- tracking validity and sample age;
- hand-to-head distance;
- hand position in HMD-local coordinates;
- control-face-to-head dot product;
- aim direction relative to head;
- controller up vector relative to world/HMD up;
- linear velocity and recent vertical displacement;
- angular velocity and recent turn angle;
- optional finger curl/touch/open-hand confidence;
- timestamp and hand identity.

The portable recognizer should own thresholds, timing, hand selection, hysteresis, and state.

#### C. Use a sequence, not merely a pose

A robust opening gesture should express intent. Recommended first candidate:

```text
1. Controller is in a non-facing/rest state briefly.
2. One hand rises into a comfortable chest-to-head region.
3. The control face turns toward the headset.
4. That same hand remains stable for a short confirmation dwell.
5. The menu opens world-stable; the user may then relax the wrist.
```

The raise/turn transition makes an ordinary static gameplay pose less likely to trigger. Lock the recognizer to the first qualifying hand until it fires or cancels; do not reset merely because the other hand briefly qualifies.

#### D. Add hysteresis and grace

Use stricter entry thresholds and looser continuation/exit thresholds. Starting hypotheses for physical tuning, **not final values**:

- facing enter: dot `>= 0.75–0.80` (about 41–37 degrees);
- facing continue: dot `>= 0.55–0.65`;
- candidate distance enter: approximately 0.25–0.75 m;
- candidate distance exit: approximately 0.18–0.90 m;
- require a recent 0.08–0.15 m rise and/or 35–50 degree turn;
- armed/rest dwell: 200–400 ms;
- confirmation dwell: 600–1000 ms;
- tolerate 80–150 ms of brief threshold/tracking dropout without losing all progress;
- after firing, require pose exit plus 500–1000 ms cooldown before re-arming.

The existing three-second dwell should not be the principal false-positive defense. A better gesture sequence can be both safer and faster.

#### E. Filter signals before thresholding

Use timestamp-aware filtering on position, orientation-derived scalar features, and velocities. A One Euro filter is a suitable candidate because it suppresses low-speed jitter while remaining responsive during deliberate fast motion. A simpler exponential filter is acceptable for the first trace-driven prototype.

Important details:

- filter continuous signals, not only the final Boolean;
- derive angular differences with quaternion-safe math;
- use real sample intervals rather than assuming a frame rate;
- reset filter/state after prolonged tracking loss;
- do not let a stale last-valid pose continue dwell progress;
- preserve a small dropout grace so one bad sample does not restart the gesture.

#### F. Add optional intentionality signals carefully

Potential optional gates, in order of practicality:

1. **capacitive touch pattern** while raising the wrist;
2. **skeletal summary** such as index extension/open-hand confidence;
3. **small explicit button confirmation** after the pose is recognized;
4. **gaze-at-wrist** only as an optional experiment, not a requirement.

Do not require skeletal or eye data until availability, focus behavior, comfort, and accessibility are physically proven. A pose-plus-button confirmation may be very reliable, but only if dashboard/overlay input delivery is solved without stealing an important game control.

#### G. Improve feedback

- Do not show progress until the recognizer reaches `Candidate`; this prevents visual noise during ordinary movement.
- Place the progress indicator near the candidate hand or just above the eventual menu, not at central gaze.
- Use color/state changes for candidate, confirmed, and canceled.
- If an output action is available, add one brief low-amplitude haptic pulse on candidate lock and a stronger pulse on activation.
- Once triggered, place the menu world-stable and release the wrist constraint, as the current design already intends.

### 4.3 User-adjustable policy

Persist only user-facing policy, not raw device-specific matrices in the portable model unless calibration establishes a need:

- gesture enabled/disabled;
- preferred hand: left, right, either;
- sensitivity preset: conservative, balanced, responsive;
- confirmation dwell;
- optional “require raise motion” and “require touch/finger gate” switches;
- reset gesture calibration.

Default should be conservative until false-positive tests pass. The dashboard tab must remain available when gestures are disabled.

---

## 5. Proposed implementation plan for a future session

### Phase 0 — Physical evidence capture

No product behavior changes.

1. Retest the existing dashboard/system-button behavior on PC SteamVR and native Steam Frame.
2. Confirm where third-party dashboard icons appear in SteamVR 2.17.10.
3. Capture logs showing action `bActive`, origin, binding, and action-set update errors with experimental overlay overrides both off and on.
4. Record short pose traces for natural gameplay and intentional wrist opens.
5. Confirm the controller local-axis/control-face assumption.

Deliverable: a small evidence table with runtime, backend, active game type, launch order, dashboard state, and results.

### Phase 1 — Dashboard launcher prototype

**Implementation status:** complete in code on 2026-10-03; Windows x64 and Linux ARM64 builds pass; icon, panel, and laser controls are physically approved on Windows/VRLink. Native ARM64 and extended lifecycle gates remain pending.

1. Add dashboard main and thumbnail handles beside the existing regular overlays.
2. Submit a static icon to the thumbnail.
3. Render a minimal dashboard panel with Show/Hide, Glance, Pin, and Settings.
4. Route dashboard mouse events to existing portable commands.
5. Update the dashboard only on state/diagnostic changes, not at video frame rate.
6. Detect active-tab/dashboard visibility to avoid unnecessary work.
7. Keep regular overlay lifecycle unchanged.
8. Test whether dashboard-tab creation behaves identically on Windows/VRLink and native ARM64.

Acceptance gates:

- system button exposes a recognizable PhoneCast icon;
- selecting it opens the PhoneCast panel;
- Show recovers from fully Hidden without a custom global binding;
- controls do not activate the underlying game;
- closing the dashboard releases input;
- the phone scene overlay remains/restores correctly;
- game transitions, dashboard open/close, sleep/wake, and receiver restart are tested physically.

### Phase 2 — Startup/lifecycle experiment

1. Test manual launch, overlay-before-game, and game-before-overlay order.
2. Evaluate `SetApplicationAutoLaunch` only after the dashboard tab works.
3. Record behavior after SteamVR restart, headset wake, native application change, and receiver crash.
4. Provide an explicit user setting for autostart; do not silently enable it.
5. Keep PC and native packaging separate behind platform services.

Acceptance gate: the launcher is present predictably, or its absence/recovery behavior is clearly documented.

### Phase 3 — Gesture observation and offline recognizer

1. Replace binary observations with a portable `GesturePoseSample`/feature structure.
2. Add bounded, privacy-safe trace logging: transforms/features and recognizer state only—never screen or notification content.
3. Collect intentional and non-intentional traces from multiple games and postures.
4. Replay traces through deterministic unit tests.
5. Implement filtering, hysteresis, candidate-hand lock, dropout grace, and cooldown in portable VR code.
6. Keep thresholds externally configurable for test builds.

Acceptance gates:

- intentional opens complete comfortably in about one second after the deliberate raise/turn;
- no triggers during a defined ordinary-play test set;
- a brief tracking glitch does not reset all progress;
- stale/lost tracking can never complete a gesture;
- either hand works without cross-hand ambiguity;
- tests cover irregular frame timing and timestamp wrap/ordering errors.

### Phase 4 — Optional Frame-specific signals

1. Add optional touch actions and haptic output actions to a test action set.
2. Test whether they are active while a scene game owns focus and while the dashboard is open.
3. Prototype skeletal summary input only if action delivery is reliable.
4. Compare false-positive/false-negative rates with and without finger/touch gates.
5. Keep pose-only behavior as the portable baseline.

### Phase 5 — Contextual visible-overlay buttons

Only after the dashboard recovery path is approved:

1. prototype a small phone-edge toolbar;
2. compare hover reveal versus click-to-open;
3. test accidental activation during phone interaction and direct grabbing;
4. ensure the toolbar never obscures remote-touch targets;
5. decide whether to ship it or rely on dashboard plus gesture.

---

## 6. Physical validation matrix

At minimum test:

| Dimension | Cases |
|---|---|
| Backend | Windows PC SteamVR/VRLink; native Linux ARM64 |
| Scene | no app; dashboard; PC VR game; native flat app; native VR scene when available |
| Launch order | PhoneCast first; game first; dashboard first |
| Hands | left; right; both tracked; one lost |
| Body pose | seated; standing; controller at side; aiming; reloading; steering; two-handed grip |
| Lighting/tracking | normal; temporary occlusion; controller wake/reconnect |
| Lifecycle | dashboard open/close; game switch; headset sleep/wake; receiver restart |
| Gesture outcome | intended success time; false triggers/hour; cancellations; tracking-drop recovery |
| Input safety | game still receives unclaimed controls; no stuck click/trigger after dashboard closes |

Record quantitative gesture metrics:

- true activations / attempts;
- median and 95th-percentile time to open;
- accidental activations per hour;
- candidate starts that cancel;
- failures due to tracking loss;
- hand ambiguity events;
- user comfort rating.

Do not mark the dashboard, buttons, or gesture complete based only on compilation or automated tests.

---

## 7. Architectural boundaries

Preserve the existing Core/VR/platform separation:

```text
Portable VR/application layer
├── launch/control commands
├── menu/dashboard view model
├── gesture feature model and recognizer
├── transactional settings
└── telemetry counters

OpenVR platform layer
├── regular overlay handles
├── dashboard main/thumbnail handles
├── overlay event translation
├── controller/pose/skeletal acquisition
└── texture submission

Platform lifecycle layer
├── manifest registration
├── optional autostart
└── Windows vs Steam Frame packaging
```

OpenVR handles, action paths, controller types, and SteamVR dashboard state must not enter Core. Dashboard and scene panels should issue the same portable commands. Gesture policy should be backend-neutral; pose extraction and controller profile transforms belong in the platform adapter.

---

## 8. Risks and open questions

### Documented

- OpenVR supports dashboard overlays with main and thumbnail handles.
- Dashboard overlays use mouse-style interaction.
- OpenVR supports regular overlays and overlay event polling.
- Frame Controllers expose native OpenVR bindings, touch/value controls, 6-DOF poses, haptics, and skeletal input.
- Experimental overlay-global priority can override scene inputs bound to the same source.

### Tested in this repository

- regular OpenVR overlays work in the current PC-hosted path;
- a native ARM64 regular overlay works on Steam Frame over an on-device flat application;
- scene-overlay laser selection has worked physically;
- the current explicit Frame action manifest/binding loads;
- controller button commands were not delivered in the latest physical test;
- keyboard-driven state changes work;
- the current pose-only gesture and its revised three-second dwell still await full physical approval.

### Unknown until tested

- whether a PhoneCast dashboard tab appears and behaves correctly on native Steam Frame;
- whether it remains available over a native VR scene application;
- whether dashboard controls reliably route to the native regular overlay;
- exact dashboard icon ordering/presentation across SteamVR versions;
- whether autostart is permitted and durable on Steam Frame;
- whether Frame skeletal/touch actions are active for this overlay across scene focus;
- whether overlay-global action priority works in the target runtime/configuration;
- the best per-hand controller control-face transform;
- gesture thresholds and false-positive rate during real games.

---

## 9. Decision

Current product decision:

- Treat the physically approved OpenVR dashboard as the dependable launcher/recovery path.
- Do not attempt unsupported native Quick Access injection.
- Do not make experimental global button overrides mandatory.
- Keep wrist-pose and thumbstick menu gestures disabled.
- Defer extended UI and lifecycle testing while Sprint 7 adds functional VR interaction; record issues discovered through that broader use.
- Revisit gesture recognition only if the dashboard proves insufficient.

This path is supported by the public API, minimizes game-input conflicts, fits the existing portable state/settings model, and can be shared by the Windows and future native Steam Frame OpenVR backends.

---

## Sources

Primary sources:

1. Valve OpenVR, **IVROverlay overview**  
   https://github.com/ValveSoftware/openvr/wiki/IVROverlay_Overview
2. Valve OpenVR, **CreateDashboardOverlay**  
   https://github.com/ValveSoftware/openvr/wiki/IVROverlay::CreateDashboardOverlay
3. Valve OpenVR, **ShowDashboard**  
   https://github.com/ValveSoftware/openvr/wiki/IVROverlay::ShowDashboard
4. Valve OpenVR, **SetOverlayInputMethod**  
   https://github.com/ValveSoftware/openvr/wiki/IVROverlay::SetOverlayInputMethod
5. Valve OpenVR, **HandleControllerOverlayInteractionAsMouse**  
   https://github.com/ValveSoftware/openvr/wiki/IVROverlay::HandleControllerOverlayInteractionAsMouse
6. Valve OpenVR, **SteamVR Input**  
   https://github.com/ValveSoftware/openvr/wiki/SteamVR-Input
7. Valve OpenVR, **Action manifest**  
   https://github.com/ValveSoftware/openvr/wiki/Action-manifest
8. Valve OpenVR, **SteamVR Skeletal Input**  
   https://github.com/ValveSoftware/openvr/wiki/SteamVR-Skeletal-Input
9. Valve OpenVR SDK header (`VRActiveActionSet_t`, overlay-global priorities, application APIs)  
   https://github.com/ValveSoftware/openvr/blob/master/headers/openvr.h
10. Valve Steamworks, **Steam Frame Input**  
    https://partner.steamgames.com/doc/steamhardware/steamframe/input?l=english
11. Valve Steamworks, **Custom Engines / Steam Frame Controllers**  
    https://partner.steamgames.com/doc/steamhardware/steamframe/engines/custom?l=english
12. Valve Steamworks, **SteamVR Overlay Apps**  
    https://partner.steamgames.com/doc/features/steamvr/info?language=english
13. Valve SteamVR release announcements (2.17 Quick Access/dashboard behavior)  
    https://steamcommunity.com/app/250820/announcements

Supporting gesture-design references:

14. Géry Casiez et al., **The One Euro Filter** reference implementation and paper links  
    https://github.com/casiez/OneEuroFilter
15. Unity XR Hands, **Static hand gesture component** (minimum hold time and gesture-state design)  
    https://docs.unity.cn/Packages/com.unity.xr.hands@1.4/manual/gestures/static-hand-gesture.html
16. Khronos, **XR_EXT_hand_tracking** (for comparison only; not established as supported on Steam Frame)  
    https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#XR_EXT_hand_tracking
