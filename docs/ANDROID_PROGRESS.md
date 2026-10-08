# Android development checkpoint — October 8, 2026

## Project and current milestone

SpeedBreaker One is onenonlygit's personal, AI-assisted Android ARM64 fork of
SpeedBreakerProject/speedbreaker (project(u)). Preserve upstream attribution,
GPL license, renderer/runtime and existing artwork. Android work lives on
`android/rp6-bringup`; main retains upstream code with a fork overview.

**Juan completed a full Quick Race sprint on Retroid Pocket 6 using 0.1.2.**
This is a working-gameplay checkpoint, not a stable or optimized release.

| Area | Latest evidence / limitation |
| --- | --- |
| Graphics | User reports no visibly broken graphics in this race; screenshot shows road, buildings, car, mirror and HUD. Broader accuracy unverified. |
| Quick Race | One complete sprint reported. Repeated races, other tracks/cars and long sessions not verified. |
| Career | Previous build could skip videos and reach an opening cinematic, then had missing world geometry. Latest Career progression not tested. |
| Audio | Music and some effects play. No engine/car audio, selective other sounds. Cause unresolved. |
| Performance | Racing samples around 20.6–30.1 FPS at internal 1280×720. Display swapchain 1920×1080. Not a 1080p render benchmark. |
| Memory | New meminfo: PSS 2,033,238 KiB (~1.94 GiB), RSS 2,139,372 KiB (~2.04 GiB). Track growth across races. |
| Graphics pipelines | No failed links or compatibility-fallback recoveries in either new session. |
| Presentation | Only initial swapchain setup; persistent SUBOPTIMAL accepted. Previous per-frame recreation stopped. |
| ISO | Direct reads of 49-file supported disc layout; picker permission remembered. No extraction or duplicate ISO required. |
| Suspend/resume | Deferred by user; not the present priority. |

## Engineering history

1. Established Android ARM64/SDL/Vulkan toolchain and native game APK, retaining
   SpeedBreaker's runtime/frontend. Added Android file picker and direct disc
   source/mount support rather than requiring extracted game files.
2. First game build reached menus, videos and Quick Race selection. Low-memory
   process kills, sideways presentation and Adreno pipeline rejection blocked racing.
3. `0.1.1-android-quickrace` / `da22647c89536f5b26ce82099fda4170eaff986b`:
   window-oriented identity presentation and immersive fullscreen; Android upload
   ring 64 MiB/four slots; lazy GPU-shadow page population; 32 MiB image blocks;
   one pipeline worker; bounded shader captures and memory reporting. Quick Race
   still killed. Career showed cars but missing roads. See ANDROID_QUICK_RACE.md.
4. `0.1.2-android-race` / `9809202a33be4f823202293612ee8469c984c0f4`:
   stop Android swapchain recreation on SUBOPTIMAL; ordinary vertex draw path
   separate from rectangle expansion; explicit constant initialization/redirect
   checks; pinned glslang SPIR-V optimization and validation; one older-SPIR-V
   fallback on VK_ERROR_UNKNOWN; separate cache keys; per-session failure captures;
   actual translator synthetic regression test. See ANDROID_RACE.md.
5. Device test of 0.1.2: completed sprint, world graphics restored, incomplete
   audio and low frame rate remain. New sessions: `2026-10-08_18-00-14.log`
   (race) and `2026-10-08_18-03-01.log` (second launch/menu). The supplied exit
   report lists older exits up to 17:32, not a new 0.1.2 memory-kill diagnosis.

## What the latest logs support

Racing command-processor busy time is approximately 11–16 ms/frame, with roughly
2,300–3,550 draws, 20–21 render passes and about 21 resolves/frame. GPU completion
waits dominate many logged hitches; steady racing has negligible pipeline waits
and few texture uploads. Rendering/resolve cost and synchronization are the
strongest performance leads. The `GPU busy` log is not a reliable pure shader
execution timing breakdown (`executing` currently reports zero); add/verify
GPU timestamps before asserting a specific GPU shader bottleneck.

Audio opened as six-channel 5.1 float at 48 kHz, including a six-channel device
format on Android. Inspect channel energy and stereo downmix/routing as well as
XMA and engine-specific sound decoding. File-open logs say engine `.abk`/`.gin`
paths are absent as loose files, but also report absent geometry/music paths
that visibly/audibly work via packed archives. These messages alone do not prove
missing assets or a direct-ISO bug. Trace packed ZDIR/ZZDATA lookup and actual
read/decode results before changing the disc loader. Preserve direct ISO access.

## Next work, in order

1. Restore engine/car audio: trace engine banks/GIN decoding and mixer inputs;
   verify six-channel-to-stereo output. Compare music/effects paths and avoid
   treating every loose-file miss as fatal. Josie video audio also previously absent.
2. Reduce rendering cost: profile pass/resolve work, oversized EDRAM targets,
   shader memory accesses and completion waits. Keep current graphics correctness.
   Test changes independently against this known working APK.
3. Consecutive-race stability: memory snapshots at menu/load/driving, race finish,
   next race. Determine retained caches versus growth/leaks. One race is not soak testing.
4. Then broader car/track/Career coverage and performance tuning. No unsupported
   promise of 1080p60 or a power target; suspend/resume remains deferred.

## Reproduce and continue

Start from `android/rp6-bringup`; read this checkpoint, ANDROID_RACE.md and
ANDROID_QUICK_RACE.md. Latest tested APK and checksum are committed under
`builds/android/0.1.2-android-race/`. Do not replace that APK with an untested build
under the same name/version. Exact binary metadata and device instructions live
beside it. The APK was built before this documentation commit; its source commit
is the 9809202 commit above.

Use existing Android build/dependency scripts. NDK r28c (28.2.13676358), Java 17,
Gradle 8.12, Android SDK/build tools 35, pinned SDL 3.2.24 and glslang 15.1.0 with
its known_good SPIRV-Tools/Header revisions. Release test command:

```sh
cd android
gradle --no-daemon -PsbDiagnostics=OFF -PsbBringup=ON :app:assembleRelease --max-workers=4
```

`sbBringup=ON` keeps the optimized release debuggable for `adb run-as` log capture;
it is not a debug/unoptimized renderer. Generated PPC source, supported XEX and
tool checkouts are intentionally excluded from Git; regenerate locally using
the documented scripts and the user's own game. Do not publish raw shaders,
user device logs, disc images or extracted assets. Diagnostics were analyzed
locally; this document records the relevant sanitized results.

The supported XEX SHA-256 is
`aebdf3c14d1e4e4ed2c130d94bc1a0e97bd6d5bf39bdfdef33c3909c9cc4ba4c`.
APK package: `com.onenonlygit.speedbreakerone`, version code 3.
Keep the existing private signing key/certificate for future install-over builds;
never commit the key or signing passwords. Certificate fingerprint and matching
native Build IDs are in the APK's `build.json`.

Offline validation passed: original captures validate; 72 revised cross-stage
pairs / 144 modules (three primitive modes, two targets) compile/validate;
eight synthetic modules emitted by the actual translator compile/validate;
synthetic translator test, optimized signed ARM64 build, signature and matching
symbol checks. The APK exports 56,165 translated guest functions.

Device collection must run locally: cloud cannot connect to the RP6 LAN.
Last wireless ADB endpoint was `10.0.0.109:35469`; Android can change the port.
Capture logcat before launch and exit-info before relaunch after failure.

## Follow-up phase: 0.1.3 audio/measurement (not yet device tested)

After the 0.1.2 checkpoint was saved, user authorized continuing with remaining
usage. Android playback now requests stereo while retaining six-channel guest
mixer input, so SDL downmixes before output. Functional dummy-device test against
pinned SDL passes all six individual channels. Aggregate channel RMS/non-finite
counts, XMA stats and supported GPU timestamps are enabled for Android. The
optimized signed ARM64 APK builds; no engine-audio recovery or performance gain
has been established on RP6. See ANDROID_AUDIO.md. 0.1.2 remains the last known
working device-tested APK; do not relabel it as 0.1.3 or overwrite its release.

## 0.1.4: Android viewport-area trial (October 8, 2026)

Baseline: `0acaa1b`, 0.1.3 audio-test. User reports most audio working and
21–39 FPS racing at 720p, 60 FPS menu. This supersedes the earlier untested
0.1.3 status; no new logs were reanalyzed for this change.

One optimization: Android defaults the existing `NFSMW_UNTILED_AREA` path on.
Untiled draws and dynamic-rendering areas use the clipped guest viewport rather
than the entire oversized EDRAM target. Existing checks retain whole targets
for disabled clipping, invalid viewport transforms, incompatible area unions,
pass expansion and resolves reading beyond the cut region. Target allocations,
resolve shaders, pass merge policy, barriers, submission slots, audio and ISO
loading are unchanged. Environment override `NFSMW_UNTILED_AREA=0` restores the
baseline policy. Desktop defaults remain unchanged.

Evidence: racing has ~20–21 passes/resolves and completion waits, with little
steady pipeline/upload activity. Code shows Android uses full EDRAM areas,
e.g. 1280x1024 or larger for a 1280x720 view, and full-size cube-face areas.
Reducing area can reduce overdraw and tile attachment traffic. Upstream reports
benefits on Apple but a small regression on Linux Turnip; stock RP6 Adreno is
unmeasured. This is a bounded trial, not a performance claim. Existing resolve
fallbacks do not constitute proof of graphics equivalence across every scene.

Synchronization review: completion-thread fence waits protect submission/upload
slot reuse and guest memory access. Queue-idle calls found in presenter setup,
resize/teardown are not evidence of a per-draw stall. Full inter-pass barriers
remain conservative. Draw caching and pass merging already exist; changing these
at the same time would obscure attribution. No new profiling query pools: retain
0.1.3 submission GPU timestamps and enable existing `[untiled-area]` aggregates
once per 120 frames (override `NFSMW_LOG_UNTILED_AREA=0` disables them). Reported
MB loaded+stored is a model, not a hardware bandwidth counter.

### RP6 comparison

Install 0.1.4 over 0.1.3 without clearing data. Use internal 1x (1280x720), the
same car/Quick Race/track, device performance mode, brightness and thermal state.
Warm caches with one race; measure the next identical 2-minute race. Record FPS
range/typical FPS, frame-time spikes and watts if available. Repeat the baseline
under equivalent conditions using preserved 0.1.3 (downgrade may require adb
`install -r -d` because version code is lower; keep data).

Collect full logcat and app session logs, including `[perf]` GPU executing,
CPU busy and completion/slot waits, and `[untiled-area]` Mpx/MB, pass restarts
and resolves-read-past. Check roads, car, mirror, reflections, shadows and HUD,
then verify engine audio/music and start a second race. FPS improvement alone
without comparable warm/thermal conditions is insufficient. Regression: retain
0.1.3 and revert only the Android viewport-area default.

### Offline validation and artifacts

Optimized debuggable ARM64 release build passed (existing NDK/Gradle setup).
APK v2 signature verified against retained certificate
`29ebbb37c610217ff00755e3731df6a1db46eda4cec45555157704ab545902bf`;
package unchanged, version code 5. Native libmain Build ID differs from 0.1.3.
Existing host disc-mount, SDL stereo-downmix and write-watch tests (subpages off
and on) passed; git diff --check passed. These do not exercise Vulkan rendering
on Adreno. No RP6 access or graphics/performance validation was performed.

Checksums and native Build IDs are in builds/android/0.1.3-android-audio/
(preserved baseline) and builds/android/0.1.4-android-area/ (new trial).
Both APKs are preserved in the build workspace; 0.1.4 is delivered as a download.
The authenticated source commit does not include APK binaries; shell Git push
was unavailable because this workspace has no GitHub shell credentials.
