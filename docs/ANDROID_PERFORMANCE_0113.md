# Android 0.1.13 — precomputed vertex-memory routes

## Baseline and observations

Baseline: Plume APK 0.1.12, compiled b27899e. Juan confirms Quick Race on RP6,
Plume Vulkan, System driver, 720p, with no visible glitches. Some scenes reached
40 FPS but the game remains slow. This overlaps previous 21–39 FPS observations;
there is no matched speed delta, full-race parity proof or custom-driver recovery.
Keep that APK unchanged as the device-tested fallback.

Existing 0.1.10 timings remain the last captured pass attribution: roughly
25.9 ms color/offscreen, 5 ms depth, 3.5 ms resolves, ~34 ms GPU workload. They
are evidence for selecting a candidate, not measurements of the new build.

## Source path, invariant and hypotheses

Guest fetch constants -> existing guarded/shared or ring snapshot ownership ->
translated vfetch -> vmem per word -> up to four snapshot-range checks -> byte
swap/format decode -> unchanged guest shader ALU. Four-word attributes repeat
that routing search. Every new raw word must equal the old first-match snapshot
or shared-memory source, including out-of-range and wrapped addresses.

Hypothesis A: repeated routing costs meaningful vertex/binning work across the
expensive passes. Precompute a route and share its decision across an attribute.
A neutral/regressed warm pass time with high route eligibility would weaken this.
Hypothesis B: fragments, bandwidth or driver scheduling dominate instead. The
same pass time despite cheaper routing supports moving to those paths rather
than attributing all color time to fragments. CPU preparation/cache invalidation
is a competing regression risk, so route identity excludes absolute ring offsets.

## Implementation and rollback

`vertex_route.h` computes one route per used fetch constant from existing snapshot
ranges, in original priority order. Whole-range containment selects the ring;
no snapshot overlap selects shared memory; partial/priority overlaps retain the
old per-word routing. Ring bias is relative to snapshot zero's push-constant ring
origin. Snapshot upload order, padding, fences, ownership and memory copies stay.

96 route words (384 bytes, packed into 24 uvec4s) extend the existing constants
UBO; no new descriptor, submission or synchronization. Cached constants are
invalidated if route metadata changes, not merely when the ring address moves.
The shaders check that the complete 1–4-word attribute lies within its declared,
clamped fetch range before taking the direct path. Out-of-range, zero-size,
wrapped and ambiguous accesses use original vmem for each word. Fetch format,
endian, mini-fetch addressing, relative indices, primitive expansion and guest
shader arithmetic remain unchanged.

Advanced **Vertex Memory Routing** / `SB_VERTEX_MEMORY_ROUTING=0` restores the
old per-word path after restart. Defaults follow existing Android GPU-work
policy. Both Plume and Legacy can use the optimization independently of backend
selection. Shader source hashing separates generated module variants.

Periodic `[vertex-route]` logs include draw count, declared snapshot/shared/
fallback routes, and metadata changes. These are cumulative CPU eligibility
counts, not actual GPU branch hits or shader invocation counts. Existing pass
and full-frame diagnostics remain active. No new capture/profiler is required.

## Verification

- 8.4 million randomized/boundary word-source/address comparisons against an
  independent original per-word scan pass. Covers first-match overlapping
  snapshots, zero ranges, all 1–4-word widths, high address aliases, underflow,
  512 MiB boundary, unsigned ring bias, and changing ring origins.
- Actual routed GLSL runs through Vulkan compute and compares every result with
  original vmem across 257 mixed range/address cases for both native and Plume
  recording, with Khronos core+sync validation and AddressSanitizer checks.
- All 15 supported formats compile with routing on/off, Vulkan 1.1/1.2 and split/
  contiguous guest memory: 120 glslang/spirv-val modules pass. Existing 45
  standalone host suites and four offscreen/Legacy tests pass.
- One optimized four-word synthetic shader grows from 16,824 to 18,288 bytes and
  54 to 57 static conditional branches because the complete fallback remains.
  This is not a static-code-size win. Eligible runtime branches bypass the old
  four per-word searches; host SPIR-V counts do not establish Adreno cycle savings.
- Optimized Android ARM64 native link and signed release packaging pass. Compiled clean source `64fe727`; retained certificate, seven exact staged libraries, 56,736 game mappings, ZIP/alignment/manifest checks pass. Build record: `builds/android/0.1.13-plume-vulkan/`.
- Clean GitHub Actions offscreen run [39274316020](https://github.com/onenonlygit/speedbreaker-one/actions/runs/39274316020) passes.

No RP6 execution or speedup is claimed for 0.1.13. GPU coverage is software
Vulkan on the host; it does not establish game visual parity.

## Next acceptance test

Use the same Quick Race, System driver, Plume backend, 720p and fixed quality/
scheduling/shader settings. Warm 60 seconds, drive two minutes, then repeat with
Vertex Memory Routing OFF after restart. Check cars, world geometry, shadows,
mirror, HUD and audio. Retain existing pass CSVs, performance windows and route
logs; compare warmed pass/frame times and CPU core time. Reject a regression or
visual difference. High metadata churn/low direct eligibility means the CPU/
branch tradeoff may lose; do not interpret a single peak FPS as success.

### Additional 0.1.12 driver observation

Juan subsequently tested Mr Purple T26 on the same prior build: reached the
50 FPS range, with some drops to 26 FPS. This establishes that this driver can
run the tested session; it does not establish universal custom-driver stability,
matched driver gains or the cause of the drops. Keep T26 fixed for an ON/OFF
vertex-routing comparison; do not compare a new T26 run to an old System run
and attribute the combined difference to renderer code.
