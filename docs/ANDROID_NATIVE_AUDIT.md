# Native rendering audit — October 22, 2026

Source baseline: `a42fd33` on `android/plume-rhi`. The working tree was clean
after restoring the published branch. README history cleanup did not change
implementation files. This audit precedes the next game-call integration stage.

## What actually runs

| Boundary | Source evidence | Status |
| --- | --- | --- |
| Playable Android app | Launcher/settings select Legacy or Plume; `command_processor.cpp::StartOnce` starts the PM4 worker unconditionally | Compatibility rendering remains the gameplay path |
| Plume integration | `plume_backend.cpp` records production graphics/compute operations and owns renderer images | Implemented, beyond dependency linking |
| Native renderer core | `InitializeNative`, `NativeDraw`, native clear/copy/readback/present in `renderer.cpp` | Implemented; explicit state/images, no CP register array |
| Native submission | `native_worker.cpp` supplies recording, submissions and completion futures | Implemented; called by native bridge/fixtures, not game startup |
| CPU device bridge | `native_device.h` tracks surface generations/bindings and captures CPU mirrors for draw calls | Implemented; runtime has no game caller of this bridge |
| Game shader resources | `game/graphics_shaders.cpp` hooks shader constructors and declaration patching | Connected to the current renderer; does not replace game draw submission |
| Native game resources/draws | No original game surface/draw/clear/resolve functions call the device bridge | Integration gap |
| Guest CPU/GPU contracts | Existing write-watch and PM4 fences/events/queries remain | Native lock materialization, aliases and guest synchronization are incomplete |

The native core is functional in host fixtures. A fully native gameplay backend
is not selectable, and no packaged APK uses that core for the complete game.
The last packaged checkpoint remains 0.1.15 (code 16); later native source work
has not been packaged or measured on RP6.

## Validation boundary

The previous checkpoint records three native-runtime CTest cases and six
Plume/Legacy cases passing. They exercise real Vulkan/Plume operations, exact
pixels, translated synthetic vertex microcode, depth/stencil, submission,
native fences, resource reuse and presentation image retirement. ASan and
core/synchronization validation were used for native-runtime cases.

Those results do not execute original game bodies or validate Adreno, gameplay
coverage, CPU lock faults, custom drivers or performance. The fixture's memory
platform uses immutable-memory stubs. Tests must be rerun for integration changes;
the earlier results are not evidence that newly added hooks work.

## Continuation selected by this audit

The next missing boundary is game resource lifetime and binding, rather than
another RHI wrapper. Verify the supported USA surface constructor, target setters
and deallocator against regenerated original bodies; connect those entry points
to an explicitly attached native device. Keep original CPU bookkeeping and
returned context intact. Test real wrappers, generation reuse, binding and GPU
retirement with original-call fixtures, including disabled pass-through.

Follow with draw/UP state and patched shader selection, native texture aliases,
cubes/mips and format conversion, CPU materialization, guest fences/events/queries
and presentation. CP startup can be disabled only after those contracts have
replacements. A partial resource-hook stage must not expose a playable-native
selection or be packaged as the requested native gameplay APK.

Invariants: one loader/device/queue; one ordered renderer owner; no stale image
after object reuse; explicit unsupported cases; no dropped draws sold as FPS
improvement. Existing GPU timestamps and earlier RP6 measurements remain the
performance baseline. No new profiling system or claimed speedup is needed to
establish this integration gap.

## Continuation implemented after the audit

Added strong USA wrappers for `82589FC8` (surface construction), `8258D920`
(render-target binding), `8258DC88` (depth binding) and `8262E9F0` (game heap
free). Regenerated supported USA bodies confirm constructor arguments/return,
the format word at surface+52 and final bindings at device+12536/+12552.
The heap-free wrapper retires only objects tracked as native surfaces; other
allocations retain original handling. Original callees run exactly once, with
their returned PPC context preserved. No generated game code is committed.

An explicit attachment connects these wrappers to the native CPU device only
after native worker startup. The selected guest device's resulting bindings
are read after original CPU bookkeeping. Detach excludes in-progress hook work
and retires all tracked images. Current Legacy/Plume gameplay leaves attachment
disabled. This resource stage does not yet route game draws, disable CP startup,
or expose a complete native game backend.

The full renderer fixture checks actual image creation/clears/readback/retirement
through these wrappers plus a draw using a hook-established binding. It checks
full context return, null allocation, address reuse, different image dimensions,
color/depth format decoding, unrelated device bindings/heap frees, detach and
disabled pass-through. Native MSAA is rejected explicitly because the current
native image descriptions support one sample; silently reducing sample count
would violate visual acceptance criteria.

Both complete native cases and the timing-hook case pass after rebuilding with
ASan, core and synchronization validation. They now execute seven normal and one
indexed Plume draw with zero compute dispatches and zero validation errors.
One attempted run encountered a truncated empty executable; its apparent
zero-second CTest success was discarded. Explicit completion-message checks
were added for every native-runtime test, and real ELF executions were rerun.
The cause of workspace executable truncation is not established by this audit.

Next: index-buffer metadata/lifetime and exact shader/declaration binding at
draw/UP entry, then scissor/tiling-independent state, resolve aliases and guest
CPU/GPU synchronization. The original CPU surface constructor still performs
its allocation bookkeeping. This audit does not claim a playable native APK,
an Android rebuild, RP6 execution or any performance gain from these hooks.

## ClearF continuation

Verified USA `8259A308` now forwards semantic image clears after original CPU
bookkeeping when a native device is attached. Source tracing establishes this
ABI: r3 device, r4 flags, r5 optional single RECT pointer, r6 float4 color pointer,
f1 depth and r8 stencil. r5 is not a rectangle count; r7 is the integer argument
slot occupied by depth in the ABI. Input registers/depth are captured before the
original routine clobbers them; its returned context is preserved.

Color bits 0–3 select attachments; bit 4 clears depth and bit 5 clears stencil.
The rectangle intersects the CPU viewport at device+12808 and optional scissor
at +12832 (enable +11952). The default rectangle uses target dimensions or the
verified primary-frame extent at +13476/+13480 when the primary bindings match.
Native clears clip to each image's dimensions and remain image-resident.

Current support is canonical RGBA8 and normalized depth/stencil. Unsupported
color-format conversions, non-normalized depth and null color for an actual
color clear fail explicitly. Multisampled images remain unsupported. Format
support is checked before any member of a multi-target clear is recorded.
Source also shows that a nonempty color mask selecting no live color targets
emits no depth clear; this boundary is preserved instead of applying depth
flags unconditionally. Unbound depth aspects are ignored.

The GPU fixture verifies exact pixels for rectangle/viewport/scissor clipping,
default and primary-frame rectangles, ignored unrelated-device calls, depth-only
and stencil-only changes, stencil preservation, no-op unbound color selection,
and rejection before a partially applied multi-target clear. All three native
CTest cases pass with explicit completion markers, ASan and core/sync validation.
The six existing Plume/Legacy tests also passed with completion checks during
this audit. The original game bodies remain source-verified rather than executed
in the fixture. Draw/UP routing, texture aliases and the other gameplay contracts
listed above remain open; this clear subset is not a complete native game backend.
