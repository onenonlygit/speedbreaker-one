# Game-native renderer migration — 0.1.15 shader resources

## Goal and actual state

Juan explicitly chose a game-native renderer with no emulated GPU on October 19.
The target is game graphics calls -> native resource/state objects -> Plume ->
Vulkan. Renaming the existing PM4/Xenos path is not completion of that goal.

**Full conversion is NOT complete.** 0.1.14 implements functional hardware vertex
input in the running rendering flow, with validated offscreen draws and an
Android build. 0.1.15 adds game-level shader creation and immutable variants,
consumed by pipeline creation. PM4 processing, register-state translation, shader translation,
EDRAM target interpretation and custom resolves remain active. There is no
misleading fully-native backend selection. Legacy/Plume rollback stays available.

Baseline: signed 0.1.13 compiled 64fe727. Juan reports still about 30–40 FPS and
possibly better pacing, explicitly uncertain/placebo. No confirmed gain. Preserve
that APK and 0.1.12; the earlier T26 50 FPS peaks/26 FPS drops are a different
observation, not evidence that 0.1.13 improved the baseline.

## Implemented native geometry

Translated shader analysis records static attributes: fetch constant, byte
stride, signed byte offset and 1–4 raw words. For ordinary geometry whose fetch
indices still come from the original vertex ID, a new shader variant reads
`uvec4` Vulkan vertex inputs instead of storage-buffer guest-memory fetches.
The original endian swap, format decode, skinning/transforms, predicates and
shader ALU remain. This is hardware fetching of raw attributes, not CPU format
conversion or a completed original-container shader library.

- Actual VkVertexInputBinding/AttributeDescriptions and Plume vertex-buffer
  commands are used, with uint formats checked against device capabilities.
- Sources are existing fence-owned ring snapshots or protected CPU-owned shared
  buffer ranges. Imported/shadow/ring buffers include VERTEX_BUFFER usage.
- Declared range, actual uploaded index min/max, offset, stride, address wrap,
  precision and whole-attribute bounds are checked before native selection.
  Bounds use the immutable uploaded indices, not a second read of mutable guest
  memory. The float index is restricted below 2^22 to preserve rounding exactly.
- Ambiguous snapshot overlaps, GPU-owned/pending ranges, memexport, relative or
  modified indices, non-flat control flow, primitive expansion, restart, excess
  attributes/stride and unsupported host formats retain the original draw path.
  An index write after the final fetch does not unnecessarily reject a shader.
- Native and compatibility pipeline/module cache keys differ. Optional native
  compilation keeps rendering through the ready compatibility pipeline; it
  doesn't skip draws or spend the ordinary async pipeline wait budget.
- Ownership/fences and upload copies remain. Native attribute reads are included
  in pass-end synchronization. Existing resolve dependency checks cover matching
  vertex ranges; GPU-owned ranges are excluded from native binding.
- **Advanced > Native Vertex Input**, restart required, provides rollback.
  Default follows Android GPU work policy. Backend selection is independent.

`[native-geometry]` logs report candidates, native main draws, attributes, scanned
indices, pending optional pipelines and cumulative CPU preparation microseconds.
The existing pass CSV appends `native_geometry_draws`. These show coverage and
preparation cost; they do not assert shader-cycle savings. The complete legacy
fallback remains, and CPU scanning may offset a hardware-fetch saving.

## Validation and competing explanations

Hypothesis A: storage-based vertex pulling contributes to the expensive ~25.9 ms
color/offscreen and ~5 ms depth categories in the earlier capture. Hardware input
removes that access path for eligible geometry. Hypothesis B: fragments, memory
traffic elsewhere, driver scheduling or CPU preparation dominate; a matched warm
run with substantial native coverage but unchanged/regressed frame time weakens
A as an optimization priority. Do not infer vertex cost from a color-pass label.

- Native input planner tests compare exact addresses for randomized strides,
  offsets, ranges and index bounds; test eligibility and original-index writes.
- 46 standalone host suites pass. SDL-linked audio/PPC write-watch integration
  harness is outside that standalone set, as in previous checkpoints.
- Four Vulkan/Legacy tests pass under core+sync validation and AddressSanitizer.
  The graphics pilot adds 1–4-word hardware inputs with a nonzero buffer offset,
  stride, endian swap and indexed draw. All seven offscreen outputs match the
  original 1,024-byte image. Borrowed Plume buffers survive wrapper destruction.
- 480 native/compatibility format shaders compile/validate: all 15 supported
  guest formats, Vulkan 1.1/1.2, split/contiguous memory, routing on/off, and ordinary/position-only outputs.
- One synthetic four-word shader: compatibility 13,912 bytes/35 static branches;
  hardware-input variant 6,620 bytes/16 branches (ordinary primitive mode on both), with guest buffer binding zero
  absent. This demonstrates removal of that shader access path, not Adreno
  cycle performance or hot-game coverage.
- Existing fetched Plume checkout upgrade and repeatable patch-series application
  pass; clean pinned source build tests pass. One loader/device remains.
- Android optimized native link and signed APK packaging pass. Compiled clean source
  17ddb60, code 15, retained certificate, seven exact staged libraries and all
  56,736 mapping entries verified. ZIP/alignment/manifest checks pass; see
  builds/android/0.1.14-plume-vulkan/. CI was queued at verification time; no
  CI success is inferred from local validation.

No 0.1.14 RP6 execution, parity, FPS/pacing improvement or 2x speedup is established.

## Game-call boundary work and remaining blockers

Read the native reference at codepdbh/nfsmw-android commit
507a63f7d21c9b8e2d6f15d3ed0e12b9b2c38712 (GPLv3). Its native draw/target code
is useful, but its source also explicitly retains a ring sink and a staged
Direct3D mirror/marker bridge. Its label is not proof of zero PM4 work.
Its ReXGlue context, shader-library ABI and PAL addresses are not drop-in APIs
for this XenonRecomp USA runtime.

Static checks against the locally generated code identify USA PS constructor
0x8259BA98, VS constructor 0x8259BE40 and swap routine 0x825987E0 (calls VdSwap).
The reference PAL entries are respectively 0x8259BC90, 0x8259C038 and 0x825989D8.
These confirmed pairs do not authorize applying one global offset to all game
functions. No speculative hooks or generated game code are committed.

Specific next implementation stages:

1. Verify and intercept shader constructors/declarations, device/resource creation,
   state setters and draw/clear/resolve entrypoints in the USA executable. Preserve
   original shader-container identity before D3D patches fetch instructions. Map
   full object layouts and reference/lifetime behavior, not just function names.
2. Introduce native resource handles and ordered render jobs at that game-call
   boundary. Preserve all graphics state and immutable draw data. Jobs must not
   consume an emulated GPU register file as their lasting API. The current game,
   CP and presenter threads must not touch renderer state concurrently.
3. Replace EDRAM-address target identity with game render-target/texture objects.
   Retain native images across passes and implement copy/resolve semantics on
   those images; materialize bytes for legitimate CPU locks/reads and buffer
   consumers. Aliases, partial copies, depth packing and invalidation require
   explicit handling, not deletion of existing stores.
4. Own native submission/presentation and replace ring read-pointer/event/query
   behavior needed by the game with native synchronization. Keep pause, audio,
   ISO loading, input and vblank behavior. Then disable command-processor startup
   for the new backend, with counters/assertions proving no PM4/EDRAM fallback.
5. Validate complete race, menus, mirrors, shadows, tunnels, rain, motion blur,
   streaming and background/resume before calling this fully native or enabling
   a no-fallback default. Existing Legacy remains separately usable.

The missing game-level resource/state boundary and native target ownership are
real remaining implementation work. This hardware-input slice must not be
presented as completion of those stages.

## Game shader resources — October 19, 2026

Baseline: `b578ee5`, preserved 0.1.14 geometry APK. No new device measurement
was supplied. The architectural gap was shader resources known only through
IM_LOAD and mutable guest code, without original game-object identity. This
stage implements shader creation from item 1 above; the rest remains incomplete.

`runtime/game/graphics_shaders.cpp` overrides verified USA functions:

| Entry | Contract / action |
| --- | --- |
| `8259BA98` | PS constructor: original contiguous container r3; returned object r3; physical allocation pointer object+12 |
| `8259BE40` | VS constructor: same input/return; executable pointer object+40 |
| `825A2DC0` | Declaration patcher: VS object r4, executable destination r5; capture after the original patches in place or into a ring copy |

Checked against regenerated USA bodies from the supported XEX, not just a PAL
offset. Wrappers always call the original and preserve its returned PPC context,
allocation/refcounts and driver bookkeeping while creating host-owned resources.

Data path: original 2005 container -> immutable original identity -> executable
snapshot at constructor/patcher -> endian conversion and translation on the game
thread -> native shader resource -> exact executable match during existing
binding -> graphics pipeline creation and Plume drawing. Physical code begins at
virtualSize. Reject 2008 headers, wraparound, truncated/misaligned headers and
excessive sizes. This feature writes no shader data to disk.

The library keeps declaration variants separate, deduplicates matching bytes,
checks stage/length/content after hashing, and retains no guest pointers. Ring
reuse and later in-place patches cannot alter an existing resource. Unknown
constructors invalidate stale identity at reused object addresses. Resource
accounting is bounded at 64 MiB and object-identity storage at another 16 MiB.
Failed translations/budget exhaustion retain compatibility translation; failed
variants are cached to avoid repeated translation at each patch.

Renderer initialization publishes the fixed translation policy before game
threads translate. Hooks issue no Vulkan commands and never touch CP state.
**Advanced > Game Shader Resources** (`SB_GAME_SHADER_RESOURCES`), restart
required, provides rollback. Shader math and pipeline keys are unchanged.

Competing hypotheses: moving creation could reduce CP-critical translation;
duplicate capture or already-warm caches could instead add CPU work without
changing GPU time. Exact GLSL equality is the cheapest correctness discriminator.
Paired startup and warm racing timings with the setting toggled discriminate
performance. No RP6 improvement is established by this work.

Validation after workspace recovery:

- 49 host suites pass, including write-watch, resource lifetime/concurrent lookup
  and actual PPC-context hook fixtures. SDL downmix is outside this standalone
  harness; audio code is unchanged.
- The hook fixture links real wrappers/context types with synthetic original
  callees, comparing every returned context byte, variants, reuse and disabled
  fallback. It does not execute the original game function bodies.
- Five Release Plume/Legacy tests pass under core+sync validation. An actual
  translated game-resource VS renders indexed guest-endian hardware attributes,
  with UBO/push constants. Its 1,024 image bytes match the reference, alongside
  the seven previous rendering outputs. Generated SPIR-V validates for Vulkan 1.2.
- Debug/ASan exposed a borrowed-pipeline constructor asserting a non-null owner.
  Patch 0003 gives borrowed handles a separate nullptr constructor; the owned
  constructor retains its assertion. This is independent of game shader math.
  All five Debug/ASan tests then pass with core+sync validation. LeakSanitizer
  is disabled because this sandbox prevents its process/thread inspection;
  this is not a leak-check result.

**Still compatibility-driven:** IM_LOAD selects matching resources. PM4 draw
submission, register state, target identity, EDRAM copies/resolves, fences/queries
and presentation remain. This is not a no-emulated-GPU backend. Next: ordered
host draws with complete semantic state and native target/texture handles,
including CPU-visible resource synchronization, followed by native presentation.
A source checkpoint or synthetic GPU test is not a playable fully native game.

### 0.1.15 signed Android checkpoint

Clean compiled source `3402b25b06cd1a31dc9e217029d20796ffa99651`, Android code 16,
NDK r28c, RelWithDebInfo (-O2). Complete ARM64 runtime link and Gradle release
packaging pass with the preserved certificate. Seven exact staged libraries,
all 56,736 mapping symbols, three strong constructor/patcher hook symbols,
manifest/ABI/ZIP/alignment, single hidden Volk and no direct libvulkan dependency
are verified. Build ID `23c8284f4a5e8ea71ad3acf36b9d63c31a3c0ae7`.
APK SHA256 `9884ae8102ed071ee43225aa92a3d7bab7dd2766c67be433a3add965c4d2e070`.
See `builds/android/0.1.15-game-shader-resources/` for build evidence.

All three Plume patches were also applied twice to a clean worktree at the pin;
the result matches the built patched source byte for byte. Full Android OFF
link was not repeated; adapter compile-OFF and runtime Legacy GPU tests pass.
This is offline validation, with no RP6 gameplay, visual or FPS result. Existing
APKs remain separately saved. No ADB/device access or shader/disc uploads occurred.

## Native draw and image core — source checkpoint, October 19, 2026

Baseline `a81839b`, last packaged APK 0.1.15 (unchanged, SHA256 rechecked).
This checkpoint has **no new APK or selectable native game backend**. It is
implementation work toward the requested native deliverable, not completion.
The running game still enters through PM4 and the existing compatibility path.

Implemented in the shared rendering core:

- `InitializeNative()` shares the presenter's device/loader, forces Plume,
  supplies no CP register array, and does not create the legacy EDRAM resolve
  or guest-memory front-buffer conversion pipelines. No second Vulkan device.
- `NativeDraw()` takes an immutable CPU state snapshot, host shader resources,
  explicit target descriptors/handles and full-width draw arguments. It reuses
  the existing shader/pipeline/geometry implementation and Plume recording.
  `StateWord/StateWords` read named native fields/constant arrays on this entry,
  while retaining the exact legacy register-array path for existing rendering.
  Native calls cannot invoke copy-mode EDRAM resolves or the tile-skip heuristic.
- `native_draw_state.h` captures verified USA `825A3EC8` CPU D3D mirrors, not a
  decoded GPU packet. Constants/fetch/bool/loop values are copied and immutable
  after submission. Separately emitted window offsets/scissors have no mirror
  address: they are deliberately not guessed. The caller supplies semantic
  scissor and the full native target dimensions. D3D pipeline encodings remain
  inputs to the shared pipeline conversion; this is not a new register file.
- Native images have a separate cache keyed by opaque allocation-generation
  handles and exact width/height/format, rather than EDRAM addresses or an
  EDRAM-derived height. Reusing a live handle with another description fails.
  Allocation continues through the existing Plume image owner. Destruction
  waits for GPU retirement, removes sampled views and invalidates bound state.
- Native semantic partial clears, same-format image copies, separate depth and
  stencil copies, 2D sampled views and explicit CPU readback are implemented.
  Copies stay image-resident: no tiled guest-memory store followed by re-upload.
  The command helpers are shared with actual Vulkan GPU tests. CPU readback
  returns canonical GPU format bytes and handles noncoherent mapped memory;
  guest endian/format/tiling conversion and lock interception remain necessary.
- `NativePresent()` copies RGBA8 directly to a presenter-owned front image on
  the shared queue, without the compatibility front-buffer compute conversion.
  Other presentation formats, cube/mip native images, format-converting resolves,
  attachment feedback and memexport are not implemented by this entry.
- Renderer owner-thread checks protect the new entry points. They require an
  ordered native worker; they are not safe to call directly from game hooks.
  Legacy initialization and draw entry remain separate and buildable.

### Invariants, alternatives and verification

Architectural invariant: a native draw must have explicit shader/image resources
and immutable state; no CP array, EDRAM address lookup, tiled resolve store or
PM4 fallback may supply missing native work. Legitimate CPU consumers must still
receive correct resource bytes. Zero-copy deletion of those stores without
native lock/alias ownership would violate that invariant.

Hypothesis A: image residency and explicit target extents will remove some of
the previously measured resolve/memory/pass overhead. Hypothesis B: the expensive
color work remains dominant even after those costs disappear. Current evidence
proves neither performance claim. Exact image comparisons are the cheapest
correctness discriminator; only subsequent matched RP6 racing timings can test
performance. No new profiling tools or device capture were introduced.

Verification actually run:

- 50 standalone host suites pass, including CPU-mirror ABI offsets, immutable
  snapshots, VS/PS constant boundaries, overflow rejection and unmapped fields.
  SDL downmix remains outside this standalone harness.
- Six Release GPU/Legacy tests pass with core and synchronization validation.
  New color/depth/stencil tests use the same native command helpers as the
  renderer. They test unequal image dimensions, offset partial copies,
  depth-only/stencil-only clears, exact pixel values, undersized readback buffers,
  out-of-range rectangles and self-copy rejection. Zero validation errors.
- The same six tests pass in Debug with AddressSanitizer. LeakSanitizer remains
  disabled due to process-inspection restrictions; this is not a leak check.
- Optimized Android ARM64 native compilation and complete libmain link pass.
  APK packaging/signing was deliberately not repeated for this incomplete path.
- These GPU tests exercise the shared image commands and existing synthetic
  Plume draws, **not the entire new `NativeDraw()` renderer entry**, game hooks,
  native presentation handoff, or the readback allocation/lock lifecycle.
  No RP6 execution, visual parity or FPS gain is claimed.

### Required continuation

1. Exercise the complete native renderer entry with a synthetic device/context,
   translated shader resources and explicit targets; cover sampled target reads,
   lifetime retirement, readback and presentation handoff, without a CP array.
2. Connect verified USA draw/UP, clear, resolve and resource lifetime calls to an
   ordered worker. Allocate generation handles from real game object creation;
   do not synthesize identity from EDRAM base. Capture declaration-patched shader
   variants and immutable geometry before guest memory reuse.
3. Implement texture aliases, cubes/mips, conversion resolves, guest CPU locks
   and buffer consumers. Use native readback/materialization only when required;
   the current write-watch shadow alone cannot materialize an image-only write.
4. Replace ring read-pointer/fence/event/query/presentation contracts, preserve
   vblank/pause/audio and disable CP worker startup for the new backend. Validate
   full gameplay coverage before exposing it or packaging the native deliverable.

Rollback is the previous source commit; the existing launcher Legacy/Plume
choices and all previously packaged APKs remain unchanged.

## Native owner and full renderer validation — October 22, 2026

Baseline `62dd308`; last packaged game remains 0.1.15. Workspace maintenance
removed local uncommitted work twice. The recovered source is being published
in stages so continuation no longer depends on that workspace. There is still
no selectable full-native game backend or new APK from this stage.

Confirmed defects and isolated fixes:

- Pipeline compilation looked up native shader hashes in the legacy IM_LOAD
  cache. That inserted null entries and crashed on the compiler thread. The
  pipeline now receives its actual retained vertex/pixel shader objects and
  checks stage/hash identity. Both inline and threaded compilation are tested.
- Native shader IDs were treated as globally unique although each Library
  starts at ID 1. The native cache retains immutable resource ownership and
  keys by resource identity. A real translated resource with another library's
  ID 1 now renders the expected pixels.
- Clear-pass diagnostics could read a missing compatibility register array.
  Native passes now exclude that EDRAM-only diagnostic condition; tests leave
  NFSMW_LOG_UNTILED_AREA enabled to exercise the boundary.
- The CPU GamePresent hook targeted USA DrawIndexedVertices (82593A58).
  Verified regenerated USA source identifies 825987E0, which calls VdSwap, as
  the swap routine. The corrected wrapper retains the complete returned PPC
  context. Prior CPU present/frame-interval metrics were mislabeled; existing
  GPU timestamp measurements are unaffected.

`native_worker` now owns native initialization, recording, submission and idle
submission service on a dedicated thread. Calls carry immutable state/target
values, use a bounded queue, and wait for recording before guest geometry may
be reused. The owner honors guest suspension and the existing foreground gate.
Job exceptions reach callers; owner failure releases queued waiters. Explicit
native fences use the renderer's ordered completion queue and submit before
returning their futures. Eight concurrent callers exercise startup/fences.
The owner shares the presenter's existing device, loader and queue mutex.
It does not start or consume a PM4 stream.

Required native pipelines wait for compilation even if compatibility async
shader skipping is configured. Failed native draws propagate failure instead
of silently reporting success after skipping work. Optional hardware-input
pipeline compilation retains the equivalent vertex-pulling fallback.

Actual validation in this recovered workspace:

- Complete production renderer + native worker build succeeds with Clang 18,
  Debug AddressSanitizer, pinned Plume and glslang 15.1.0.
- Both inline and one-worker pipeline tests pass on lavapipe with Khronos core
  and synchronization validation and zero validation errors. Five Plume draws
  execute; compute dispatch count remains zero.
- Exact 16x16 color/scissor pixels, image sampling, 65,538-vertex count, real
  shader translation/hardware attributes, depth and unchanged stencil, offset
  copy/readback, exception propagation, fences and target retirement pass.
- Independent Vulkan readback verifies front-image pixels before and after
  destroying the native source target. This validates presentation handoff and
  image ownership, not an Android swapchain or displayed RP6 output.
- Separate actual CPU timing-wrapper test checks original-call count and every
  returned context byte. All three native-runtime CTest entries pass.
- The fixture uses a synthetic PPC address-layout header and public compiler
  headers; it requires neither the private XEX nor generated game code. The
  existing GitHub workflow now builds/runs it and retains its test log.

See tests/native_runtime/README.md for reproduction and precise boundaries.
Immutable write-watch stubs do not test CPU lock faults or texture aliases.
Process-lifetime production threads use `_Exit` in the fixture; LeakSanitizer
is disabled, and this is not a leak check. No RP6 performance gain is claimed.
No Android compilation/packaging was repeated for these recovered changes.

The next gameplay integration still requires real game surface allocation
generations, patched shader bindings, draw/UP/clear/resolve hooks, texture
aliases/cubes/mips and CPU materialization, then replacing guest fence/event/
query/ring contracts and disabling CP startup. The owner/fence implementation
and validated GPU operations provide those hooks' execution destination;
the current game still uses the compatibility renderer until that migration.
