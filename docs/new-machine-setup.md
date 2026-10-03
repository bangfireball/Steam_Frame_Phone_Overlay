# New Windows Development Machine and Pi Agent Setup

This is the migration checklist for rebuilding the PhoneCast VR development environment on another Windows PC. `docs/development.md` remains the detailed build and runtime reference; this document covers machine and agent bootstrap.

## 1. Transfer sensitive state safely

The repository contains no Pi credentials or Telegram secrets. Do **not** add them to Git.

Choose one of these approaches:

- preferred: authenticate again with Pi `/login`, Git Credential Manager, and Telegram on the new PC;
- migration: securely copy only the needed files from `%USERPROFILE%\.pi\agent`, especially `auth.json`, `telegram.json`, `settings.json`, and any private custom skills.

Treat `auth.json` and `telegram.json` as secrets. Do not send them through chat, commit them, or place them in the project directory. Pi sessions under `%USERPROFILE%\.pi\agent\sessions` are optional history and are not needed for a new agent to understand the repository.

## 2. Install Windows prerequisites

Run from an elevated PowerShell terminal where appropriate:

```powershell
winget install --id Git.Git -e
winget install --id OpenJS.NodeJS.22 -e
winget install --id Kitware.CMake -e
winget install --id Ninja-build.Ninja -e
winget install --id BrechtSanders.WinLibs.POSIX.UCRT -e
winget install --id Valve.Steam -e
```

Install through Steam:

- SteamVR (Steam app `250820`);
- Steam Frame/VRLink support offered by the current SteamVR installation.

Install Android Studio when Android sender work is required. In SDK Manager install:

- Android SDK Platform 36;
- Android SDK Build Tools;
- Android SDK Platform Tools (`adb`);
- a JDK supported by the checked-in Gradle wrapper (JDK 17 or newer; JDK 21 is known to work).

Also install the Steam Frame deployment/runtime tools required by Valve when resuming native ARM64 work. The normal Windows receiver does not require those tools.

After installation, open a new terminal and verify:

```powershell
git --version
node --version
npm --version
cmake --version
ninja --version
g++ --version
java -version
adb version
```

If `g++` is not found, add the installed WinLibs `mingw64\bin` directory to the user `PATH`. If `adb` is not found, add the Android SDK `platform-tools` directory. Android Studio normally stores the SDK under `%LOCALAPPDATA%\Android\Sdk`.

## 3. Install Pi

Pi requires Node.js 22.19 or newer:

```powershell
npm install -g --ignore-scripts @earendil-works/pi-coding-agent
pi --version
```

Inside Pi, run `/login` and connect the desired provider. Use `/model` to select the model.

### Recreate the current agent packages

The following reproduces the package set used by the original development environment:

```powershell
pi install npm:pi-lmstudio
pi install git:github.com/badlogic/pi-telegram
pi install npm:pi-subagents
pi install npm:pi-web-access
pi install npm:@petechu/pi-extension-toggle
pi install npm:@bacnh85/pi-kicad
pi list
```

For PhoneCast development, `pi-subagents` and `pi-web-access` are the most generally useful optional additions. `pi-telegram`, `pi-lmstudio`, extension-toggle, and KiCad support are personal workflow tools and are not required to build PhoneCast.

If Telegram is needed, configure it on the new machine or securely migrate `%USERPROFILE%\.pi\agent\telegram.json`. Never commit that file. Custom user skills, such as `%USERPROFILE%\.pi\agent\skills\mdp`, must be copied separately if desired.

Native Windows Pi uses Git Bash for its Bash tool. Git for Windows normally supplies it automatically. Pi user configuration lives under `%USERPROFILE%\.pi\agent`; see the upstream Pi configuration documentation if a custom `PI_CODING_AGENT_DIR` is used.

## 4. Clone PhoneCast

```powershell
New-Item -ItemType Directory -Force C:\Projects | Out-Null
Set-Location C:\Projects
git clone https://github.com/bangfireball/Steam_Frame_Phone_Overlay.git vr_mobile_overlay
Set-Location vr_mobile_overlay
git fetch --all --prune
git checkout review/sprints-5.1-6-headset-validation
```

Change the branch when the work is merged or development moves elsewhere.

The repository's `AGENTS.md` instructs a fresh coding agent to read `design.md` and the relevant documentation before changing code.

## 5. Build and test the Windows receiver

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --test-dir out/build/windows-x64 --output-on-failure
```

CMake downloads the pinned OpenVR SDK source, so the first configure requires network access.

If tests fail with Windows status `0xc0000139`, a different MinGW runtime is earlier on `PATH`. Read `CMAKE_CXX_COMPILER` from `out\build\windows-x64\CMakeCache.txt`, prepend that compiler's adjacent `bin` directory, and run CTest again. See `docs/development.md` for the exact diagnostic procedure.

Expected result at the time this guide was written: all nine CTest tests pass.

## 6. Configure and build Android

Android's `local.properties` is intentionally ignored by Git. Point it at the new SDK installation, for example:

```properties
sdk.dir=C\:\\Users\\YOUR_USER\\AppData\\Local\\Android\\Sdk
```

Then run:

```powershell
Set-Location android\sender
.\gradlew.bat testDebugUnitTest assembleDebug lintDebug
Set-Location ..\..
```

Install the APK when needed:

```powershell
adb install -r android\sender\app\build\outputs\apk\debug\app-debug.apk
```

## 7. Validate SteamVR from the physical console

Do not start or restart SteamVR through RDP. RDP previously caused VRLink D3D11 texture creation failures and invalid gray/invisible test results.

From the physical Windows console:

1. start Steam and SteamVR;
2. connect Steam Frame through VRLink;
3. double-click `Start PhoneCast VR.cmd`;
4. confirm the launcher reports port `49321` and pairing code `123456`;
5. start casting from Android;
6. verify the overlay and controller controls;
7. double-click `Stop PhoneCast VR.cmd` when finished.

Logs are written under `out\logs`. Persistent overlay settings are stored in `%LOCALAPPDATA%\PhoneCastVR\overlay-settings.ini` and may be copied to retain the old machine's calibration.

## 8. Start a fresh agent

From the repository root:

```powershell
pi
```

A useful first prompt is:

```text
Read AGENTS.md and design.md completely, then review the current branch, docs/development.md, and the latest git log. Summarize current sprint status, physical validation still required, and the next safe task. Do not mark hardware behavior complete without a physical test.
```

## 9. Migration checklist

- [ ] Repository cloned and correct branch checked out
- [ ] Git credentials configured
- [ ] Node.js and Pi installed
- [ ] Pi provider authenticated
- [ ] Required Pi packages installed
- [ ] Secrets migrated securely or recreated
- [ ] CMake, Ninja, and WinLibs compiler available
- [ ] Windows receiver builds
- [ ] All CTest tests pass
- [ ] Android Studio/SDK/JDK configured
- [ ] Android tests, APK, and lint pass
- [ ] Steam and SteamVR installed
- [ ] Steam Frame/VRLink connects from the physical console
- [ ] PhoneCast launcher starts the receiver
- [ ] Optional `%LOCALAPPDATA%\PhoneCastVR\overlay-settings.ini` calibration migrated
