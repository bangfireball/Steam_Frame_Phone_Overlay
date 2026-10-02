# PhoneCast VR

PhoneCast VR aims to show an Android phone as a persistent VR overlay, first through PC SteamVR and eventually directly on Steam Frame ARM64.

The project is currently in **Sprint 0 — Hello Frame**, a feasibility experiment for persistent third-party overlays.

## Current status

- `[x]` Initial Valve/OpenVR/OpenXR research
- `[x]` Minimal OpenVR Hello Frame source
- `[x]` Windows x64 build and CLI tests
- `[!]` PC SteamVR visual/runtime test — Steam Frame was not connected
- `[x]` Linux ARM64 cross-build and emulated CLI smoke test
- `[!]` Steam Frame standalone runtime test — headset unavailable over the network

See:

- [`design.md`](design.md) — product and sprint plan
- [`docs/steam-frame.md`](docs/steam-frame.md) — evidence and open questions
- [`experiments/hello-frame/README.md`](experiments/hello-frame/README.md) — build and test instructions
