// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include "vertex_route.h"
#include <cstdint>
namespace gpu::native_vertex {
struct Attribute { uint32_t constant, stride, words; int32_t offset; };
// Fixed-function fetch must reproduce the shader's float index exactly and
// remain inside a protected guest range or fence-owned snapshot. Offsets are bytes.
inline bool Plan(const Attribute& a, uint32_t vf0, uint32_t vf1,
    const vertex_route::Snapshot* copies, uint32_t count, uint32_t first,
    uint32_t last, uint32_t indexOffset, uint32_t& bindingOffset, bool* shared = nullptr) {
    if (first>last || uint64_t(last)+indexOffset > 0x3FFFFFu || !a.words || a.words>4) return false;
    uint32_t base=vertex_route::Base(vf0),size=vertex_route::Size(base,vf1);
    uint32_t route=vertex_route::Build(base,size,copies,count);
    if(route==vertex_route::kFallback) return false;
    const bool direct=route==vertex_route::kShared;
    int64_t begin=int64_t(first+indexOffset)*a.stride+a.offset;
    int64_t end=int64_t(last+indexOffset)*a.stride+a.offset+a.words*4;
    int64_t at=(direct ? 0 : int64_t(int32_t(route)))+base+int64_t(indexOffset)*a.stride+a.offset;
    if(begin<0 || end>size || at<0 || uint64_t(at)>UINT32_MAX) return false;
    bindingOffset=uint32_t(at);
    if(shared) *shared=direct;
    return true;
}
}
