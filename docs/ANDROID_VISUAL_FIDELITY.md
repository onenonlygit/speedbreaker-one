# 0.1.5 visual diagnostics — source checkpoint, no APK built

Personal AI-assisted Android fork of SpeedBreaker; GPL and upstream artwork retained.
Baseline `0.1.4-android-area` (code 5) is preserved. Proposed diagnostic version is
`0.1.5-android-visual-diagnostic` (code 6), same package and signing identity.

## Source-backed findings

* EDRAM depth targets use `VK_FORMAT_D32_SFLOAT_S8_UINT`. A depth resolve maps
  D24S8 to texture format 22 and D24FS8 to format 23.
* `ClearRect` decodes D24FS8 as 20e4 float, but `kResolveGlsl::pack32` packs
  both 22/23 as UNORM24. `TextureFormat`, the untile convert=4 path and the
  fused resolve-to-texture path also decode both as UNORM24. This is a concrete
  format inconsistency. Captures must establish whether affected racing draws
  actually use format 23 before attributing the visual failure to it.
* Pixel shader export 61 sets `writesDepth` and writes `o_depth`; generated
  fragment GLSL never assigns `gl_FragDepth`. This is a second concrete omission;
  whether any relevant game shaders use it remains unverified. Capture records
  `pixel_writes_depth` to establish usage. Neither behavior is changed here.
* Texture fetches translate to ordinary sampler2D calls; Vulkan samplers have
  comparison disabled. Shadow comparison may be explicit shader arithmetic;
  disabled hardware comparison alone is not evidence of a bug.
* Occlusion counting handles sun-glare quads and defaults on for Android through
  `__linux__`, subject to precise-query support. Results are recorded, not altered.
* 0.1.4 changed Android viewport-area defaults; shared depth conversion and
  shader export omissions also exist in desktop code. No desktop build was made.
* Other competing causes: stale/fused resolve data, clipping/pass merging,
  coordinate/LOD transitions and final exposure/bloom compositing. Screenshot and
  video observations from the supplied task are treated as user observations,
  not independently re-analyzed images in this checkpoint.

## Implemented capture

Android enables `NFSMW_VISUAL_CAPTURE` by default; `0` disables it. Other hosts
remain disabled unless explicitly enabled. Captures arm after 30 consecutive
frames with >=1500 draws, >=10 passes and a depth resolve. This is a render-state
heuristic, **not a verified guest Quick Race flag**. Complex 3D menus could
trigger it, and unusual low-complexity tracks could fail to trigger. No launch
clock delay, device connection, Career progression or manual debug controls.

Three selected frames, separated by at least 60 swaps, use the existing
NFSMW_PASS_PROFILE query infrastructure. Capture writes under
`files/data/speedbreaker/diagnostics/visual-<unix-timestamp>/`:

* `manifest.json`: recording/waiting/complete, attempted frame count, last frame,
  cumulative bytes written and I/O error flag. Complete means three capture
  attempts finished, not that every requested resource succeeded.
* `render-passes.csv`: GPU and gap times, result status, draw count, resolves,
  uploads, submissions, memory synchronization KB and attachment descriptions
  (EDRAM identity, guest/Vulkan formats, dimensions and rendering area).
* `draw-state.jsonl`: up to 6000 draws/frame; shader hashes, depth/blend/color
  state, viewport transform/scissor, six raw fetch words per sampled slot,
  depth-export flag, and first 32 referenced constants/stage as raw float bits.
  This constant cap can omit some projection/lighting constants; shader-relative
  reads use the first 32. Occlusion records contain count and originating frame.
* `depth-targets/`: up to four depth resolve source snapshots/frame **before
  clear**, little-endian float32 raw files, PNG previews and tiled resolved guest
  words. Summary describes dimensions, formats, endian, pitch, base and extent.
* `resolved-textures/`: end-of-frame R32F textures whose exact guest base was
  depth-resolved and sampled that frame. Usage, not a 1600-pixel size alone,
  selects these candidates. They are representations at frame end, not guaranteed
  copies of the precise contents at each earlier draw. Address-offset aliases,
  color-rendered shadow maps and non-R32F candidates are not captured here.
* `frame-<id>-final.png` and `.rgba8`: exact renderer front image for that selected
  frame. `final-frame.png` aliases the latest successful PNG; manifest last_frame
  identifies the latest attempted frame, last_final_frame identifies its alias. This is before presenter scaling/UI.
* `summary.txt`: metadata, unsupported/oversized image reports, raw value range,
  unavailable timestamps and capture limitations. Occlusion detail is also logged.

Each image is limited to 12 MiB of raw pixels, at most 12 images/frame. Binary
and draw output share a cumulative 140 MiB budget (overwrites count too), with
1 MiB reserved for summary/manifest. No unlimited shader dumps or game assets.
Float previews map clamp(value,0,1)*255 directly: **no reversed-depth inference
or automatic normalization**. Use raw files and depth state when interpreting.
No matched sampled texture means the exact-base heuristic found none; it does
not prove no shadow sampling occurred.

Vulkan snapshots end any active render pass, retain GENERAL layout, use the
existing coherent mapped staging ring, synchronize preceding GPU writes, issue
transfer-to-host barriers and wait for submission fences before reading. No
resource pointers escape to background tasks; render targets are never written
by capture. Readbacks split submissions and deliberately stall selected frames,
so these timings are diagnostic, not performance benchmarks. Texture images gain
TRANSFER_SRC usage only when capture is enabled. Existing shader source, audio,
ISO access, saves, renderer feature switches and visual algorithms are retained.
Timestamp pool creation is capability-checked and nonfatal when unavailable.

## Validation and exact APK blocker

Passed: entire renderer translation-unit GCC host syntax check with downloaded
pinned SDL/glslang and Vulkan headers, compile-only PPC fixture, and temporary
GCC compatibility shims for two upstream Clang-specific declarations. This is
**not Android compilation or a desktop executable build**. Portable capture test
checks gameplay gating, three-capture stop, budget exhaustion without empty
binary creation, metadata reserve, and JSON completion. Existing ZPD tests pass.
Collector passes bash syntax validation; collector was not executed in Work.
`git diff --check` passes. No GLSL/SPIR-V shader code changed; no new shader
compiler or Adreno validation run is claimed. Synchronization was source-reviewed,
not exercised on a Vulkan device. Audio/ISO code is untouched, not retested here.

The supplied private JKS was opened privately and its certificate SHA256 matches:
`29ebbb37c610217ff00755e3731df6a1db46eda4cec45555157704ab545902bf`.
No replacement key generated. No credentials were put in Git or logs.

**No 0.1.5 APK exists.** Full build blockers in this available workspace:

1. Neither actual generated `ppc/` nor the supported user-owned `default.xex`
   survived; previous working APK cannot regenerate missing guest sources.
2. Android SDK 35 / NDK r28c, Gradle distribution and ARM64 dependencies absent.
3. Gradle wrapper download of 8.12 fails with `Network is unreachable` for
   services.gradle.org in Work. GitHub source-header downloads worked; they do
   not restore Android binaries or generated game code.

Do not publish a prerelease APK or label this source as Android build-validated.
Restore the existing private game/build inputs in an authorized build environment,
then use the pinned preparation scripts and this command:

```sh
cd android
./gradlew --no-daemon -PsbDiagnostics=OFF -PsbBringup=ON \
  :app:assembleRelease --max-workers=4
```

Supply the retained SB_KEYSTORE/SB_STORE_PASSWORD/SB_KEY_ALIAS/SB_KEY_PASSWORD
privately. Verify package, code 6, ARM64 runtime and certificate with apksigner;
record SHA256 and Build ID before delivery. Do not substitute the platform probe.

## Minimal later RP6 test

Once a genuinely built/signed APK is delivered, install over 0.1.4 without clearing
data. On the Linux desktop with an already configured ADB device, run:

```sh
scripts/android/capture_visual_fidelity.sh
# Optional: ANDROID_SERIAL=<existing-device> SB_CAPTURE_TIMEOUT=300 ...
```

Start Quick Race and drive for roughly 10–15 seconds after loading. The script
waits up to five minutes and packages logcat, new diagnostics, exit info, package
and memory reports into a timestamped `.tar.gz`. It excludes old completed
sessions, uses exact `files/...` paths rather than a mismatched `./files/...`,
and reports extraction errors. If no capture arms, the timeout archive is still
useful; do not enter Career just to satisfy the heuristic. Keep diagnostic
archives private; do not attach game-derived data to a public GitHub release.
