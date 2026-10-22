# Android 0.1.14 — first native geometry slice

Signed optimized ARM64 release APK (code 15, API 33+), compiled clean source
17ddb60603b186b669a3d2b615279f8a77bb0d17. Same package/certificate, no game assets
or driver bundled. Release enables run-as diagnostics. Later commit only records
build evidence and corrected compiler comparison.

**This is NOT yet a fully native renderer.** It adds actual hardware vertex
input for eligible geometry. PM4 processing, register/shader translation,
EDRAM target interpretation/resolves and compatibility fallback remain active.
Full-native migration stages and game-call boundary details are documented in
docs/ANDROID_NATIVE_RENDERER.md. No full-native backend label is exposed.

Plume Vulkan remains the default backend; selected driver preferences remain.
Advanced > Native Vertex Input defaults ON on Android; OFF and restart rolls
back this slice. It binds protected CPU-owned buffers or fence-owned snapshots
through native attribute layouts. Raw format decoding/endian and shader ALU stay.
Unsupported geometry and pending optional pipelines retain the ready old path.
No draw is deliberately skipped to improve FPS. Existing pass CSVs append native
geometry draw counts; native coverage and preparation time are logged.

Validation: 46 standalone host suites; four Vulkan/Legacy checks with core+sync
validation and AddressSanitizer; all seven offscreen images match, including
1–4-word hardware inputs, endian, offsets, strides and indices. 480 ordinary/
position-only native/compatibility shader variants compile/validate. A fair
ordinary-mode synthetic comparison is 13,912 bytes/35 branches vs native
6,620/16, not the earlier comparison that included compatibility expansion.
These are source/host results, not Adreno performance or game-image parity.

Optimized full Android Plume link and signed packaging pass: retained v2 cert,
ZIP and 16KB packaging alignment, version/ABI, all seven byte-identical staged
libraries, 56,736 game mapping entries, single hidden Volk, no direct libvulkan
import, and included notices. Existing Plume checkout upgrade/patch-series
application passes. Full Android OFF link was not repeated for this version;
current compile-time OFF adapter and runtime Legacy GPU tests pass.
CI was queued: https://github.com/onenonlygit/speedbreaker-one/actions/runs/39278410134

0.1.12 and 0.1.13 APK checksums verified unchanged. No 0.1.14 RP6 launch, game
parity, FPS/pacing improvement, driver stability or 2x speedup is established.
Next work is game-level graphics/resource interception and native target
ownership before disabling the command processor and EDRAM compatibility.
