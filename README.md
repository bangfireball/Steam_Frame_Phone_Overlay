# PhoneCast VR

PhoneCast VR aims to show an Android phone as a persistent VR overlay, first through PC SteamVR and eventually directly on Steam Frame ARM64.

**Sprint 0 — Hello Frame** is complete. The foundational native Steam Frame overlay mechanism was accepted after user-visible coexistence with an on-device standalone 2D game. Sprint 1 is next.

## Current status

- `[x]` Initial Valve/OpenVR/OpenXR research
- `[x]` Minimal OpenVR Hello Frame source
- `[x]` Windows x64 build and CLI tests
- `[!]` PC SteamVR visual/runtime test — Steam Frame was not connected
- `[x]` Linux ARM64 cross-build and emulated CLI smoke test
- `[x]` Native Steam Frame deployment and overlay creation
- `[x]` User-confirmed visible, head-locked overlay during a standalone 2D game
- `[x]` Sprint 0 foundational feasibility accepted
- `[!]` Follow-up: validate coexistence with a standalone VR scene application

See:

- [`design.md`](design.md) — product and sprint plan
- [`docs/steam-frame.md`](docs/steam-frame.md) — evidence and open questions
- [`experiments/hello-frame/README.md`](experiments/hello-frame/README.md) — build and test instructions
