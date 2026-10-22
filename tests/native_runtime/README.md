# Complete native renderer fixture

Runs the production renderer, Plume adapter and dedicated native owner against
CPU Vulkan. No PM4 decoder, GPU register array, EDRAM resolve pipeline, game
executable, generated game code or captured shaders are linked. Public XenonRecomp
headers still supply the runtime's context ABI; `fixture/ppc_config.h` uses a
synthetic address layout exclusively for this test.

Prepare public dependencies:

```sh
bash scripts/android/prepare_recompiler.sh --headers-only
git clone --depth 1 --branch 15.1.0 https://github.com/KhronosGroup/glslang.git tools/glslang-android
cmake -S tests/native_runtime -B build/native-runtime -G Ninja \
  -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_BUILD_TYPE=Debug -DSB_NATIVE_SANITIZE=ON
cmake --build build/native-runtime -j4
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/native-runtime --output-on-failure
```

Requires a Vulkan loader, software Vulkan driver, Khronos validation layer,
X11 development headers, CMake, Ninja and Clang. Select lavapipe with
`VK_ICD_FILENAMES` if another device is the default. Both inline and asynchronous
pipeline compilation run with core and synchronization validation.

Checks exact color/scissor, sampled image, 65,538-vertex count, real translator
hardware vertex input, library-local shader IDs, depth/stencil, offset copies,
readback, presentation handoff/source retirement, exception propagation,
native GPU fences and concurrent callers. The CPU device bridge adds actual
draw/indexed submissions from verified driver mirrors, surface object generation
reuse and overflowing index-range rejection. Six draws, one indexed draw and zero
compute dispatches must execute through Plume. Shadow-memory stubs copy immutable fixture geometry;
CPU lock faults, alias materialization and gameplay bodies remain outside this
test. Presentation checks GPU front-image bytes before/after source destruction,
not an Android swapchain or displayed device pixels. Production
threads live until process exit; `_Exit` avoids concurrent static destruction.
LeakSanitizer is disabled; no device leak or Adreno performance claim follows.
