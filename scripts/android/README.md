# SpeedBreaker One — Android bring-up

Personal, AI-assisted fork. SpeedBreaker's CPU, kernel, Vulkan/Xenos renderer,
XMA decoder and patched XenonRecomp are retained. No ReXGlue or game assets.

## Status: GO WITH RISKS

A game-free Android platform probe builds. All 73 runtime translation units
compile with NDK r28c using compile-only header fixtures. Android SDL3, glslang
and patched FFmpeg XMA libraries build. Actual generated PPC code, full-runtime
linking, device launch and gameplay are **not validated**.

The probe tests the real shared-memory backend: a 4 GiB guest reservation,
512 MiB shared backing, bidirectional A/C/E aliases with exact +4 KiB E shift,
mprotect/SIGSEGV retries, atomic CAS/futex wake, SDL startup, Android Vulkan
surface and present queue capability. It logs GPU/driver, renderer extensions,
BC support, controllers and lifecycle. It does not create a game swapchain,
exercise the complete write-watch algorithm, play XMA audio or run the game.

Remaining risks: SELinux/shared-memory behavior, real concurrent write-watch,
stock Adreno Vulkan extensions and BC texture formats, surface replacement on
resume, RAM pressure, audio and performance. The upstream renderer maps Xenos
compressed textures directly to BC formats; missing BC support needs a texture
adaptation or compatible driver. No concrete fundamental blocker found yet.

## Build

Linux x86-64 host, Java **JDK** 17, CMake 3.22+, Ninja, Clang 18+, make, git,
curl, Python and Android SDK. Target API 33+, arm64-v8a. NDK r28c
(28.2.13676358), SDL 3.2.24, glslang 15.1.0, FFmpeg 7.1.1.

```sh
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/28.2.13676358"
sdkmanager 'ndk;28.2.13676358' 'platforms;android-35' 'build-tools;35.0.0' 'cmake;3.22.1'
scripts/android/prepare_deps.sh --probe-only
cd android
./gradlew -PsbDiagnostics=ON assembleDebug
```

Compile the runtime without game code:

```sh
scripts/android/prepare_recompiler.sh --headers-only
scripts/android/prepare_deps.sh
cmake -S android -B build/android-check -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-33 \
  -DSB_ANDROID_DIAGNOSTICS=ON -DSB_ANDROID_COMPILE_CHECK=ON
cmake --build build/android-check --target runtime-compile-check -j 6
```

The compile-only target uses arbitrary PPC layout constants and emits **objects
only**, never a linked runtime. Those fixtures never enter an APK.

The intended full build needs your own USA base `default.xex`, 3,936,256 bytes,
SHA-256 `aebdf3c14d1e4e4ed2c130d94bc1a0e97bd6d5bf39bdfdef33c3909c9cc4ba4c`.
Only this executable is needed at the code-generation boundary; a full ISO is
not needed for building. All game assets are required on-device later.

```sh
scripts/android/prepare_recompiler.sh
scripts/android/generate_ppc.sh /path/to/your/default.xex
scripts/android/prepare_deps.sh
cd android
./gradlew -PsbDiagnostics=OFF assembleDebug
```

This full build path remains **unverified** until actual PPC sources are supplied.
The default full configuration fails loudly with no `ppc/`. Public CI builds only
the probe and runtime compile-check. Never publish game-derived code/objects,
`default.xex`, ISO or extracted assets. Keep upstream's generation model.

## Adaptations

Memory uses memfd_create/ftruncate, with ASharedMemory fallback, then MAP_FIXED
only inside the already-reserved guest range. The first Android target requires
4 KiB host pages and preserves the +4 KiB E alias. Larger pages fail explicitly.
A 4 GiB virtual reservation does not consume 4 GiB of RAM.

NDK r28 libc++ lacks atomic_ref. Android guest uint32 words use compiler atomics
with original sequentially consistent defaults and alias-aware shared futex
wait/notify. Desktop and Apple retain std::atomic_ref. Render-queue fences,
PPC sync/lwsync/eieio, FPCR and all other XenonRecomp patches remain unchanged.
API 33 supports existing backtrace calls. Full write-watch stays enabled.

SDLActivity provides landscape/fullscreen/cutout support. Upstream SDL lifecycle,
gamepad and audio paths remain. Android runtime stderr is mirrored to logcat tag
SpeedBreakerOne. Saves/game/logs live under internal `data/speedbreaker/`, caches
under `cache/speedbreaker/`, imports under `imports/`. The initial importer scans
that private folder (debug adb run-as can populate it); SAF UX is not implemented.
The runtime starts with NFSMW_RENDER_SCALE=1 for 720p bring-up. Android shared-
library symbol extraction needs refinement: upstream's ELF reader uses
/proc/self/exe, which is the app launcher, not libmain.so.

## Signing

The delivered APK uses a dedicated, retained private SpeedBreaker One RSA key.
Its backup and credentials are separate from git. Never regenerate it per build.
Export SB_KEYSTORE (absolute JKS path), SB_STORE_PASSWORD, SB_KEY_ALIAS and
SB_KEY_PASSWORD; both debug/release use the key when configured. Diagnostic
release builds are debuggable for report extraction; normal game releases are not.

CI secrets: SB_KEYSTORE_BASE64 (same JKS, base64), SB_STORE_PASSWORD,
SB_KEY_ALIAS, SB_KEY_PASSWORD. Until these are provisioned CI produces an
**unsigned release APK**, not a disposable signature. No secrets are committed.

## RP6 platform test

```sh
adb install -r SpeedBreaker-One-arm64-diagnostics.apk
adb logcat -c
adb shell am start -n com.onenonlygit.speedbreakerone/.SpeedBreakerActivity
# Close the summary; move sticks/triggers, press buttons; background/resume once.
# Android Back exits.
adb shell run-as com.onenonlygit.speedbreakerone cat files/android-diagnostics.log > android-diagnostics.log
adb logcat -d -v threadtime > speedbreaker-one-logcat.txt
```

Send both files. If it exits before the report exists, send full logcat plus:
`adb shell dumpsys activity exit-info com.onenonlygit.speedbreakerone`.
This APK cannot test menus, races or FPS. Those tests start after generated code
is integrated, the full runtime links and the USA game is installed.
