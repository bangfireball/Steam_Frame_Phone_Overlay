# Development

## Receiver prerequisites

- Git
- CMake 3.24 or newer
- Ninja
- A C++17 compiler
- SteamVR for receiver runtime testing

CMake fetches the OpenVR 2.15.6 source pinned at commit `0924064316de3effbcd1acf1e309182a2deb1c05`.

## Windows x64 receiver

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --test-dir out/build/windows-x64 --output-on-failure
```

Run SteamVR, then:

```powershell
.\out\build\windows-x64\bin\phonecast-receiver.exe --duration-seconds 10
```

The build places `openvr_api.dll` and `phonecast-receiver.vrmanifest` beside the executable.

Useful options:

```text
--width N
--height N
--fps N
--overlay-width M
--distance M
--alpha A
--duration-seconds N
```

Use `--help` for ranges and defaults.

## Linux ARM64 receiver cross-build

Install an AArch64 GNU compiler, then configure with the checked-in toolchain:

```bash
cmake -S . -B out/build/linux-arm64 \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64-gcc.cmake
cmake --build out/build/linux-arm64
file out/build/linux-arm64/bin/phonecast-receiver
```

The result should identify as an AArch64 ELF. Deploy the executable and adjacent manifest with Steam Linux Runtime 3.0 ARM64 (Sniper). See `docs/steam-frame.md` for established Steam Frame deployment findings.

## Android sender

Requirements:

- Android SDK 36
- JDK 17 or newer
- An Android 10+ device for runtime validation

The sender includes a Gradle wrapper. From the repository root:

```powershell
cd android\sender
.\gradlew.bat testDebugUnitTest assembleDebug lintDebug
```

Install the debug APK on a connected device:

```powershell
adb install -r app\build\outputs\apk\debug\app-debug.apk
adb logcat -s PhoneCastCapture
```

See `android/sender/README.md` and `docs/android-capture.md` for behavior and permission requirements.

## CMake options

- `PHONECAST_BUILD_RECEIVER` — build the Sprint 1 receiver (default `ON`).
- `PHONECAST_BUILD_HELLO_FRAME` — retain the Sprint 0 experiment (default `ON`).
- `BUILD_TESTING` — build/register tests (default controlled by CTest, normally `ON`).

## Tests

Receiver unit tests use fake logging and overlay implementations, so they do not require SteamVR. Android configuration tests also run without a device. Neither suite replaces visual compositor checks or physical-device capture/encoder tests.
