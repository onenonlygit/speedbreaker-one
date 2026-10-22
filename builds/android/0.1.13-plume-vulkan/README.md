# SpeedBreaker One Android 0.1.13 — vertex-memory routes

Signed optimized ARM64 release, API 33+, code 14, compiled clean source
64fe727a7a0e154a60fc11323eba0c790102fcd2. Same package/signing certificate;
no game assets or custom drivers bundled. Release permits run-as diagnostics.
The subsequent build-record commit only changes documentation.

Precomputes a safe memory route per fetch constant and shares the decision
across a complete vertex attribute. Keeps the old per-word scan for partial
snapshot overlaps, ambiguous priority and out-of-range/wrapped accesses.
Relative ring bias preserves constants reuse. Shader arithmetic, geometry,
formats/endian, EDRAM, synchronization and quality settings stay unchanged.

**Advanced > Vertex Memory Routing** defaults ON on Android; OFF plus restart
restores the old path. Plume/Legacy selection remains available in the launcher.
Keep driver choice fixed during comparison. Juan's previous 0.1.12 T26 report:
50 FPS range with some 26 FPS drops. No claim that the new build improves that.
The retained 0.1.12 APK SHA256 is verified unchanged.

Validation: 8.4M source/address comparisons, GPU comparison of actual routed
GLSL, all 15 formats in 120 validated shader modules, 45 host suites, four
Plume/Legacy offscreen tests with zero core+sync errors and AddressSanitizer.
CI: https://github.com/onenonlygit/speedbreaker-one/actions/runs/39274316020
Full optimized Android Plume native link and packaging pass. Compile-time OFF
adapter and runtime Legacy tests pass; a separate full Android OFF build was
not repeated for this version (both full configurations passed in 0.1.12).

APK verification: v2/retained certificate; ZIP and 16KB packaging alignment;
manifest/version/ABI; all seven byte-identical staged libraries; all 56,736 game
mapping symbols; one hidden Volk implementation; no direct libvulkan dependency;
Plume, VMA, Volk and Vulkan Header notices included. Libraries were already
stripped by the NDK before Gradle's additional-strip warnings.

No 0.1.13 RP6 execution, visual parity or measured speedup. The complete fallback
increases static shader code size; CPU metadata work and driver register/branch
cost may offset the reduction. Use the retained pass timings and route logs to
accept or reject the candidate. See docs/ANDROID_PERFORMANCE_0113.md.
