# Hello Frame

Minimal Sprint 0 OpenVR overlay experiment. It generates a static 640×240 RGBA image in memory, creates a regular OpenVR overlay, and places it 1 metre ahead of the HMD.

It intentionally has no networking, Android code, video decoder, GUI toolkit, graphics API, or production abstraction layer.

## Prerequisites

- Git
- CMake 3.24+
- Ninja or another CMake generator
- C++17 compiler
- SteamVR for runtime testing

The build fetches the pinned OpenVR SDK 2.15.6 at commit `0924064316de3effbcd1acf1e309182a2deb1c05`. Windows uses Valve's official client DLL; Linux builds `openvr_api` from source so ARM64 does not depend on a prebuilt SDK binary.

## Windows x64 build

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
```

Run SteamVR first, then:

```powershell
.\out\build\windows-x64\bin\hello-frame.exe
```

For an automatic smoke-test shutdown:

```powershell
.\out\build\windows-x64\bin\hello-frame.exe --duration-seconds 10
```

## Linux ARM64 build

On a native ARM64 Linux host, including Steam Frame development environments:

```bash
cmake --preset linux-arm64
cmake --build --preset linux-arm64
```

For cross-compilation, provide an ARM64 CMake toolchain file instead of assuming the preset changes architecture:

```bash
cmake -S . -B out/build/linux-arm64 \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64-gcc.cmake
cmake --build out/build/linux-arm64
```

Verify the result before deployment:

```bash
file out/build/linux-arm64/bin/hello-frame
ldd out/build/linux-arm64/bin/hello-frame
```

Deploy the binary using SteamOS Devkit Client with runtime **Steam Linux Runtime 3.0 ARM64 (Sniper)**. Exact tested deployment steps must be added to `docs/steam-frame.md` after hardware validation.

## Runtime test order

1. Start SteamVR without a game and run Hello Frame.
2. Start Hello Frame before a VR game; verify it survives game launch.
3. Start Hello Frame after a VR game; verify it appears.
4. Open and close the SteamVR dashboard.
5. Exit the game; verify the overlay process remains alive.
6. Stop SteamVR; verify clean runtime-loss handling.

A PC-streamed test using Steam Frame as a headset is **not** proof of native standalone overlay support.

## Application manifest

`manifest/hello-frame.vrmanifest` is provided for registration and launch experiments. Direct execution is sufficient for the first PC test. If registering it, use SteamVR's application-manifest API or the runtime's supported registration tooling and record the exact command used.

## Expected output

```text
┌──────────────────────────┐
│                          │
│       HELLO FRAME        │
│                          │
└──────────────────────────┘
```

The actual generated texture uses a compact built-in bitmap font and a blue rectangular border.
