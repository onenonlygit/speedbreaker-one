# Android 0.1.15 — game shader resource migration

Native-renderer development checkpoint; full conversion is incomplete. No RP6
FPS improvement, gameplay or visual validation is claimed for this build.

Original game shader constructors and the VS declaration patcher now create
immutable host resources. Pipeline creation reuses their translations on exact
code matches. Supported geometry continues using Plume hardware vertex input.
PM4 draw/state, register interpretation, EDRAM targets/resolves and presentation
still use compatibility handling. This APK is not the completed native renderer.

- APK: `SpeedBreaker-One-arm64-0.1.15-game-shader-resources.apk`, 21,203,047 bytes.
- Compiled clean source: `3402b25b06cd1a31dc9e217029d20796ffa99651`.
- Android version code 16; package and signing identity unchanged.
- SHA256: `9884ae8102ed071ee43225aa92a3d7bab7dd2766c67be433a3add965c4d2e070`.
- Native Build ID: `23c8284f4a5e8ea71ad3acf36b9d63c31a3c0ae7`.
- Optimized ARM64 release, NDK r28c; debuggable for existing run-as diagnostics.
- Plume default; existing backend/driver preferences remain available.

Validated: full Android native link and signed packaging; exact staged libraries,
all mappings and strong hooks; signature/ABI/manifest/ZIP/16 KiB alignment;
49 host suites (including actual PPC-context wrapper tests and write-watch);
five Release and five Debug/ASan Plume/Legacy tests with core+sync validation.
The new translated resource shader's indexed rendering matches all reference
image bytes. LeakSanitizer is disabled for restricted process inspection.
Clean pinned-source patch application, repetition and parity with built source pass.
No new full Android OFF link or successful CI result is inferred from local checks.

Install over the previous APK to keep ISO permission, saves and settings. Existing
0.1.12–0.1.14 packages are separately retained. Roll back the new shader path with
**Advanced > Game Shader Resources OFF**, then restart the app process. Launcher
Legacy Vulkan remains available. GPU timings/coverage logs remain functional.

This stage does not establish a racing speedup and does not require another FPS
capture from the user. Next development work: ordered native draw/state jobs,
native texture/target ownership and copies, then native synchronization/presentation
and removal of command-processor startup. See docs/ANDROID_NATIVE_RENDERER.md.
