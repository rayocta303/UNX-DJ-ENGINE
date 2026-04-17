# iOS Porting Plan: XDJ-UNX-C

This document outlines the strategy for porting the XDJ-UNX-C project to iOS as an unsigned IPA, suitable for installation via TrollStore or on jailbroken devices without an Apple Developer account.

## 1. Technical Goals
- **Platform**: iOS (arm64)
- **Framework**: Raylib (Core graphics/input)
- **Audio**: Low-latency CoreAudio integration
- **Target**: Unsigned IPA for TrollStore

## 2. Phase 1: Environment & Tooling
Since the primary development environment is Windows, we will use a cross-compilation approach using **Zig** or a **GitHub Actions** CI/CD pipeline.

### Option A: GitHub Actions (Recommended)
Use a macOS runner to leverage the official Apple SDKs and Clang.
- **Workflow**:
  1. Trigger build on push.
  2. Install dependencies (Zig, Raylib source).
  3. Compile for `arm64-apple-ios`.
  4. Package into `.app` and then `.ipa`.
  5. Upload artifact.

### Option B: Local Windows Cross-Compilation
Use **Zig** to cross-compile C/C++ to iOS.
- **Command**: `zig c++ -target aarch64-macos-none -isysroot <ios_sdk_path> ...` (Requires a copy of the iOS SDK).

## 3. Phase 2: Raylib for iOS Integration
Raylib officially supports iOS, but it requires a different lifecycle compared to desktop.

### Raylib iOS Lifecycle
On iOS, apps cannot use a standard `while(!WindowShouldClose())` loop. Instead, they must yield control to the OS.
- **Header**: Use `#if defined(PLATFORM_IOS)` to wrap lifecycle code.
- **Core Loop**: Move the logic inside `src/main.c` to a callback-based structure if using the standard Raylib iOS template.

### Compiling Raylib for iOS
1. **Download Raylib Source**: We need to compile Raylib from source for the `arm64-apple-ios` target.
2. **Library Selection**:
   - Use `libraylib.a` (Static link) to avoid dynamic library signing issues.
   - Link against standard iOS frameworks: `UIKit`, `QuartzCore`, `CoreGraphics`, `CoreAudio`, `AudioToolbox`, `AVFoundation`.

## 4. Phase 3: Project Structure Adaptations
### Assets Handling
iOS apps store assets inside the `.app` bundle.
- **Path Resolution**: Use `GetBundlePath()` (via Objective-C) to find the `assets/` directory at runtime.
- **Bundling**: Ensure the `assets/` folder is copied into the `Payload/xdjunx.app/` directory.

### Audio Engine (SOLA)
The current C engine (`src/audio/engine.c`) is portable but needs a platform-specific driver.
- **Driver**: Implement an AudioUnit callback or use Raylib's internal `raudio` if it provides sufficient low-latency control.
- **Buffer Management**: iOS prefers 32-bit Float PCM, which matches our internal `CSAMPLE` type.

## 5. Phase 4: Packaging (The "Unsigned" IPA)
TrollStore bypasses CoreTrust, so we don't need `codesign`.

### Packaging Steps
1. **Create Payload Directory**: `mkdir Payload`
2. **Create .app Bundle**:
   - `xdjunx` (Executable)
   - `Info.plist` (App metadata: BundleID, Version, DisplayName)
   - `assets/` (Fonts, Icons, Splash)
   - `Icon.png` (App icon)
3. **Zip & Rename**:
   - Compress `Payload` folder.
   - Rename `Payload.zip` to `XDJ-UNX.ipa`.

## 6. Phase 5: Testing & Deployment
1. **Transfer**: Move the `.ipa` to the iPhone via iCloud, AirDrop, or USB.
2. **Install**: Open with **TrollStore**.
3. **Validation**:
   - Verify touch input (Raylib translates touch to mouse by default).
   - Verify audio output via lightning/USB soundcards (CoreAudio handles this).
   - Verify Storage access (iOS File Provider).

## 7. Immediate Next Steps
1. [ ] **GitHub Action**: Create `.github/workflows/ios-build.yml`.
2. [ ] **Info.plist**: Create a basic `Info.plist` for the app.
3. [ ] **Raylib Header**: Verify `raylib.h` version compatibility for iOS.
4. [ ] **Main Entry Point**: Adapt `main.c` to allow an asynchronous loop on iOS.
