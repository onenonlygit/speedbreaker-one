// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include <cstdint>
#include <string>
namespace gpu::vertex_route {
constexpr uint32_t kSharedSize = 0x20000000u;
constexpr uint32_t kFallback = 0xFFFFFFFFu, kShared = 0xFFFFFFFEu;
struct Snapshot { uint32_t base, size; };
// Match the snapshot upload's masked address and clamped declared extent.
constexpr uint32_t Base(uint32_t vf0) { return vf0 & 0x1FFFFFFCu; }
constexpr uint32_t Size(uint32_t base, uint32_t vf1) {
    const uint32_t declared = ((vf1 >> 2) & 0xFFFFFFu) * 4u;
    return declared < kSharedSize - base ? declared : kSharedSize - base;
}
// A ring route is relative to pc.vfRedirect[0].z, so a new ring allocation
// doesn't invalidate the constants upload. Snapshot order is first-match-wins.
inline uint32_t Build(uint32_t base, uint32_t size, const Snapshot* copies, uint32_t count) {
    if (!size || base >= kSharedSize || size > kSharedSize - base) return kFallback;
    uint32_t relative = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const auto& s = copies[i];
        if (uint64_t(base) < uint64_t(s.base) + s.size && uint64_t(s.base) < uint64_t(base) + size) {
            if (base >= s.base && uint64_t(base) + size <= uint64_t(s.base) + s.size)
                return relative - s.base; // aligned: cannot equal either sentinel
            return kFallback; // partial/priority overlap must keep per-word routing
        }
        relative += (s.size + 255u) & ~255u;
    }
    return kShared;
}
// Exact GLSL used by the translator and the shader validation/equivalence tests.
// One bounded decision per attribute, rather than up to four searches per word.
inline std::string Glsl() {
    std::string s;
    for (unsigned n = 1; n <= 4; ++n) {
        s += "uvec4 vertexWords" + std::to_string(n) + "(uint address, uint vf0, uint vf1, uint route) {\n";
        s += "uint a = address & 0x1FFFFFFFu, base = vf0 & 0x1FFFFFFCu;\n";
        s += "uint size = min(((vf1 >> 2) & 0xFFFFFFu) * 4u, 0x20000000u - base);\n";
        s += "uint d = a - base;\n";
        s += "if (route != 0xFFFFFFFFu && d <= size && " + std::to_string(n*4) + "u <= size - d) {\n";
        s += "if (route == 0xFFFFFFFEu) return uvec4(";
        for (unsigned i=0;i<4;++i) s += (i<n ? "memLoad((a + " + std::to_string(i*4) + "u) >> 2)" : "0u") + std::string(i<3?", ":");\n");
        s += "uint ringWord = (pc.vfRedirect[0].z + route + a) >> 2;\nreturn uvec4(";
        for (unsigned i=0;i<4;++i) s += (i<n ? "g_ring[ringWord + " + std::to_string(i) + "u]" : "0u") + std::string(i<3?", ":");\n}\n");
        s += "return uvec4(";
        for (unsigned i=0;i<4;++i) s += (i<n ? "vmem(address + " + std::to_string(i*4) + "u)" : "0u") + std::string(i<3?", ":");\n}\n");
    }
    return s;
}
}
