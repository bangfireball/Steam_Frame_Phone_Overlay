# Windows x64 platform

The Windows receiver uses the shared OpenVR overlay backend and Valve's Windows x64 `openvr_api` client DLL. Packaging is defined by CMake; Win32 and graphics APIs are not exposed to Core.

Sprint 13 adds `WasapiAudioOutput` for opt-in phone playback through a shared,
non-ducking media session. The VR composition root uses OpenVR's output hint or
the system default; `--audio-device ID` selects an explicit WASAPI endpoint.
Mute/gain affects PhoneCast only, never game/system volume. Output failure is
non-fatal to video. Physical routing, audibility, and A/V acceptance remain pending;
see `docs/phone-audio.md`.
