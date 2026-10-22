# Plume Vulkan implementation — Android 0.1.12

## Native draw/image core — unshipped source checkpoint

The core now has an explicit native draw entry, host shader/target handles,
semantic image clears/copies, canonical readback and direct RGBA8 presentation.
Native initialization has no CP register array and skips the legacy EDRAM resolve
and front-buffer conversion pipelines. Existing Legacy/Plume rendering remains.

50 host suites and six Release plus six Debug/ASan GPU/Legacy tests pass; the
Android ARM64 runtime compiles and links. GPU tests validate the shared image
commands, not the complete native renderer entry or game execution. See
`ANDROID_NATIVE_RENDERER.md` for scope, invariants, evidence and exact gaps.
Game calls, resource aliases/locks and native fence/query ownership are still
unconnected. **Full native gameplay is incomplete; no new APK is packaged.**

## 0.1.15 native resource development checkpoint

Game shader constructors and declaration patching now create immutable host
shader resources, consumed by existing graphics pipeline creation on an exact
executable match. See `ANDROID_NATIVE_RENDERER.md` for verified USA entries,
ownership, rollback, tests and remaining architecture. This is shader resource
migration; it does not bypass PM4 draws/state or replace EDRAM target identity.

The offscreen pilot now renders an actual translated game-resource shader via
Plume hardware vertex input, descriptors and indexed drawing. It compares all
1,024 image bytes to the reference. Five Release tests (including Legacy and
compile-OFF fallback) pass with core+sync validation; 49 host suites pass.
Patch 0003 separates borrowed pipeline construction from owned-device assertions
found by Debug/ASan testing. The complete three-patch series applies to the pin
and populated checkouts. No RP6 performance gain is inferred from host results.
All five tests also pass in Debug with AddressSanitizer and core+sync validation;
LeakSanitizer is disabled for the sandbox's restricted process inspection.

The earlier 0.1.12–0.1.14 APKs remain separate preserved artifacts. Optimized
signed ARM64 0.1.15 code 16 is built from clean source `3402b25`; retained
certificate, seven staged libraries, all 56,736 mappings and strong game hooks
are verified. SHA256 `9884ae8102ed071ee43225aa92a3d7bab7dd2766c67be433a3add965c4d2e070`.
Record: `builds/android/0.1.15-game-shader-resources/`. The interrupted build before
workspace maintenance is excluded. No RP6 gameplay or performance validation.

Branch: `android/plume-rhi`; fallback: `android/rp6-bringup` and retained 0.1.11 APK.
Plume: `renderbag/plume@e48a55be0cff0f1e82b25cd0ab68be37357ee0f9` (MIT).

## Implemented rendering path

This is an incremental Vulkan interoperability implementation, not just a linked dependency. PM4 and Xenos shader translation still feed the existing renderer, which now routes these real game operations through `gpu/plume_backend.cpp` when Plume is selected:

| Operation | Implementation |
| --- | --- |
| Color/depth dynamic rendering, draw/indexed draw, compute dispatch | Plume command lists wrapping the existing fenced command buffers |
| Pipeline binding, push constants, viewport/scissor/depth bias | Plume commands with borrowed pipeline/layout wrappers |
| Graphics/compute pipeline creation, descriptors with dynamic offsets/push descriptors | Patched Plume Vulkan interop preserving original Vulkan create information and pipeline caches |
| Renderer images: render targets, depth, textures, front buffer | Plume-owned VMA allocations; original image descriptions, formats and view interpretation |
| Queue submission | Plume interop under the existing submission mutex, foreground gate and fences |
| Xbox 360 EDRAM resolves | Original custom shaders, packing, tiled guest writes and sequencing; dispatch through Plume |
| Shader translation, descriptor/pipeline layouts, guest buffers, imports, upload rings | Existing implementation |
| Barriers, timestamps, queries, transfers/readback, swapchain/presentation, remaining dynamic state | Existing Vulkan implementation |

The small pinned upstream patch supplies borrowed device/command-buffer support and native interop where the generic RHI cannot represent existing state exactly. It never creates another instance, device, queue, loader or global dispatch table. A borrowed device cannot create an independently synchronized queue. Plume has one private allocator; image owners retain its lifetime through existing fenced resource retirement. Borrowed pipeline/command wrappers do not destroy their native handles.

The generic Plume resource/shader/graphics/compute interfaces are also exercised in the offscreen validation pilot. Production game pipelines currently use the exact-state interop path. This distinction matters: broad command routing does not yet replace all state translation with generic RHI descriptions.

## Build and rollback

`SB_PLUME_RHI=ON` fetches the exact pin/submodules and applies patches 0001–0003
in `patches/plume/` idempotently, including upgrades of populated checkouts.
Plume and the runtime share that pin's Volk and Vulkan headers, with one hidden
Volk implementation. Plume's own Volk implementation is disabled. The selected
system/custom Android loader remains authoritative.

Gradle accepts `-PsbPlume=ON`; the established external native staging workflow remains supported. OFF builds do not link Plume and tolerate a stale saved Plume selection by using Legacy.

The Android launch screen's **Select rendering backend** allows Legacy Vulkan, Plume Vulkan, or the in-game Advanced setting. The selection is independent of driver preferences. Restart the game process to apply a change; Resume keeps the running backend. The in-game Advanced renderer setting is also restart-bound. Plume-enabled builds default to Plume. Use Legacy from the launcher for recovery if startup fails.

ISO loading, audio, controllers, frontend, pause/suspend gates, shader/shadow fixes and existing timestamp instrumentation retain their existing paths. No ADB connection was made. The previous 0.1.11 APK was not changed.

## Validation

- Clean FetchContent configuration and patched Plume build from the pinned remote source succeeded.
- Three offscreen/legacy tests pass on Mesa lavapipe with Khronos core and synchronization validation (zero errors); also passed an AddressSanitizer build with leak detection disabled.
- A standard Plume graphics pipeline renders a 16x16 scissored image matching native Vulkan and the runtime indexed-draw adapter byte-for-byte (1,024 bytes).
- A standard Plume compute pipeline/storage descriptor produces 257 reference-checked words through native and Plume command recording.
- Mutable-format image allocation/free and borrowed instance/device/dispatch lifetime checks pass. Runtime Legacy executes without Plume command counters; the compile-time OFF adapter links without Plume.
- Synthetic shaders pass glslang and spirv-val. Existing standalone host regression harness passes 44 suites (not the SDL-linked audio or PPC write-watch integration harness).
- Full Android ARM64 optimized native link succeeds using NDK r28c and genuine generated game code. Full Android `SB_PLUME_RHI=OFF` link also succeeds. The signed release APK is complete: code 13, retained certificate, seven exact staged libraries, all 56,736 game mapping entries, ZIP/alignment/manifest checks pass. Native code is optimized; the release variant permits run-as diagnostics. See `builds/android/0.1.12-plume-vulkan/`.
- `tests/plume/README.md` documents reproducible host checks. GitHub Actions clean offscreen validation also passed for compiled source `b27899e` ([run](https://github.com/onenonlygit/speedbreaker-one/actions/runs/39271108669)).

Plume's current Vulkan `copyTextureRegion` does not implement image-to-buffer copying. The pilot retains native readback instead; production transfer/capture paths also remain native. This is an upstream API coverage limitation, not evidence that generic resolves implement EDRAM.

## Performance evidence and limitations

Prior RP6 measurements remain the baseline: racing 21–39 FPS at 720p, approximately 34 ms GPU work, 25.9 ms color/offscreen, 5 ms depth and 3.5 ms resolves; menu around 60 FPS. The user reported no visible 0.1.11 improvement and custom-driver crashes. This implementation does not claim to fix those crashes or deliver 60 FPS. Start comparison runs with the System driver.

Shader workloads and most synchronization are intentionally preserved for parity. This stage does not establish a reduction in the expensive color workload merely by routing its commands through Plume. GPU timestamps and existing pass counters remain usable. Additional periodic `[plume]` counters expose actual graphics/compute/pipeline/descriptor/image activity. The old physical Vulkan-allocation counter only counts the legacy allocator; VMA image counts must not be compared as physical allocation counts.

No RP6 execution, gameplay-image parity, Android custom-driver compatibility, thermal or performance measurement has been performed for this APK. Host software-Vulkan success is not device or game validation.

## Next implementation and acceptance steps

1. Compare the same warm 720p Quick Race with System driver in Legacy and Plume, using retained timestamps and game settings. Validate shadows, tunnels, mirrors, depth, pause/background and resume before interpreting FPS.
2. Exercise custom-driver initialization separately with crash logs; preserve the shared dispatch and loader identity checks.
3. Convert remaining production descriptor/pipeline ownership and buffer/transfer operations to generic RHI descriptions only where exact Vulkan state can be represented. Keep custom guest-memory imports and EDRAM semantics explicit.
4. Use the existing pass timings to target color/depth memory traffic, shader/vertex work and redundant pass transitions. Accept optimizations only with image parity and lower full-frame time; 720p60 requires less than 16.67 ms, not a faster isolated pilot.

## Subsequent device report — October 19

Juan tested signed 0.1.12 in Quick Race with Plume Vulkan, System driver, 720p,
and reported no visual glitches. Some scenes reached 40 FPS but the game remains
slow. This updates the build-time absence of device evidence above; full parity,
matched performance delta and custom-driver compatibility remain unverified.
0.1.13 targets vertex-memory routing; see ANDROID_PERFORMANCE_0113.md.

Further 0.1.12 report: Mr Purple T26 reaches the 50 FPS range but sometimes drops
to 26 FPS. This is positive execution evidence for that driver/session, not a
matched performance gain or a resolution of all custom-driver crashes.

## Full-native target and 0.1.14

The next goal is a game-call-level renderer with no PM4/EDRAM emulation. 0.1.14
adds real hardware vertex inputs for supported draws. It does not complete that
conversion; existing command processing and EDRAM operations remain active.
See ANDROID_NATIVE_RENDERER.md for implementation boundaries and required stages.

## Native owner continuation — October 22

The complete production native renderer now runs through its own ordered owner
in the host fixture, without a PM4 processor or register array. Real Plume
draws, sampled native images, translated hardware vertex input, depth/stencil,
copies, readback, native GPU completion fences and presentation pixels pass
core/synchronization validation plus AddressSanitizer on lavapipe. Native
shader-cache/compilation defects found by that fixture are fixed. The fixture
and CI job require only public dependencies. See ANDROID_NATIVE_RENDERER.md
for exact evidence, CPU timing-hook correction and remaining game contracts.

This remains a source implementation stage: native gameplay routing and its
packaged Android APK are incomplete. The last packaged APK remains 0.1.15.
