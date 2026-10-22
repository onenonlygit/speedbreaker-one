# SpeedBreaker One Android 0.1.12 — Plume Vulkan

Signed optimized ARM64 release APK, API 33+, version code 13. Same package and
retained certificate as 0.1.11; no game assets or custom driver included.
Compiled source: b27899e4b01a7af09d6890044c240805da1a3148 (clean).
The later build-record commit changes documentation only. Prior APKs untouched.

In the launcher select **Select rendering backend → Plume Vulkan**, then restart
the game. Use **System driver** for the first comparison. Legacy Vulkan can be
selected before game startup for rollback. The in-game Advanced renderer setting
is used when the launcher override is cleared. Resume retains the running process;
the existing **Restart game with selected driver** button also applies the selected
backend. Restart discards unsaved race progress. Plume-enabled builds default to
Plume unless a saved setting overrides it.

Real game draws, indexed draws, dynamic rendering, compute dispatches, pipeline
binding, constants and selected dynamic state run through Plume. Image allocations
are Plume/VMA-owned. Exact-state pipeline creation, descriptors and queue submission
use patched Plume Vulkan interop. One instance/device/queue/loader remains. Xenos
translation, custom EDRAM shaders/packing, guest buffers, barriers, GPU timestamps,
transfers and presentation retain their established implementation. This is an
incremental interop migration; not all production state uses generic RHI objects.

Validation: clean pinned remote fetch/patch/build, three offscreen/Legacy tests
with zero core/synchronization validation errors, graphics byte parity, reference
compute output, AddressSanitizer run, 44 standalone host suites. Both full Android
Plume-ON and Plume-OFF configurations link. CI offscreen suite also passes:
https://github.com/onenonlygit/speedbreaker-one/actions/runs/39271108669

The final Plume APK passes v2 signature/certificate, ZIP integrity, 16KB packaging
alignment, version/ABI and all seven byte-identical staged libraries, 56,736 native
mapping entries, one hidden Volk implementation and no direct libvulkan dependency.
Includes Plume, VMA, Vulkan Headers and Volk license notices. Some already stripped
libraries trigger Gradle's additional-strip warning. Optimized release permits
run-as diagnostics, matching the established device measurement workflow.

No RP6 launch, gameplay-image parity, Turnip stability or FPS improvement is
verified. Prior 0.1.11 no-improvement/custom-driver-crash reports remain unresolved.
This build keeps the previous shader workload; merely routing commands through
Plume is not evidence of lower GPU frame time. Existing measurements remain the
comparison baseline. Plume image creation counters are not physical allocation
counts. See docs/ANDROID_PLUME_RHI.md for boundaries and acceptance steps.
