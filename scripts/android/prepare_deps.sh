#!/usr/bin/env bash
# SpeedBreaker One. GPL-3.0-or-later. Android native dependencies only.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
: "${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to NDK r28c}"
CMAKE="${CMAKE:-cmake}"
PREFIX="$ROOT/tools/android-arm64"
JOBS="${JOBS:-6}"
mkdir -p "$ROOT/tools" "$PREFIX"
if [ ! -d "$ROOT/tools/SDL" ]; then
    git clone --depth 1 --branch release-3.2.24 https://github.com/libsdl-org/SDL.git "$ROOT/tools/SDL"
fi
# SDL is built by android/CMakeLists.txt, and its matching Java is used by Gradle.
if [ "${1:-}" = "--probe-only" ]; then exit 0; fi
if [ ! -d "$ROOT/tools/glslang-android" ]; then
    git clone --depth 1 --branch 15.1.0 https://github.com/KhronosGroup/glslang.git "$ROOT/tools/glslang-android"
fi
"$CMAKE" -S "$ROOT/tools/glslang-android" -B "$ROOT/build/glslang-android" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-33 -DANDROID_STL=c++_shared \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DENABLE_GLSLANG_BINARIES=OFF \
    -DENABLE_OPT=OFF -DBUILD_TESTING=OFF
"$CMAKE" --build "$ROOT/build/glslang-android" -j "$JOBS"
"$CMAKE" --install "$ROOT/build/glslang-android"
TARBALL="$ROOT/tools/ffmpeg-src/ffmpeg-7.1.1.tar.xz"
mkdir -p "$(dirname "$TARBALL")"
if [ ! -f "$TARBALL" ]; then curl -fL --retry 3 https://ffmpeg.org/releases/ffmpeg-7.1.1.tar.xz -o "$TARBALL"; fi
printf '%s  %s\n' 733984395e0dbbe5c046abda2dc49a5544e7e0e1e2366bba849222ae9e3a03b1 "$TARBALL" | sha256sum -c -
# FFmpeg checks that no absolute machine paths are embedded: compiler names
# must be relative, resolved from PATH, not absolute --cc/--ar arguments.
export PATH="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH"
"$ROOT/scripts/build_ffmpeg_xma.sh" "$TARBALL" "$PREFIX/ffmpeg-xma" \
    --enable-cross-compile --target-os=android --arch=aarch64 \
    --cc=aarch64-linux-android33-clang --cxx=aarch64-linux-android33-clang++ \
    --ar=llvm-ar --ranlib=llvm-ranlib --strip=llvm-strip
