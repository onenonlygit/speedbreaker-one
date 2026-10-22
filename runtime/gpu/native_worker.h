// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include "native_draw_state.h"
#include "renderer.h"
#include <future>
namespace gpu::native_render::worker {
// Process-lifetime owner; start after publishing the shared Vulkan context.
bool Start();
bool Running();
// Recording completes before return, so GPU read guards protect reused inputs.
void Draw(native_render::Draw draw);
void Clear(TargetDesc target,int32_t x,int32_t y,uint32_t width,uint32_t height,
    const float* color,float depth,uint32_t stencil,bool clearDepth=true,bool clearStencil=true);
void Copy(TargetDesc source,TargetDesc dest,uint32_t sourceX,uint32_t sourceY,
    uint32_t destX,uint32_t destY,uint32_t width,uint32_t height);
void Destroy(Handle handle);
std::vector<uint8_t> Readback(TargetDesc target,uint32_t x,uint32_t y,uint32_t width,uint32_t height,bool stencil=false);
bool Present(TargetDesc source,uint32_t width,uint32_t height);
void Flush();
// GPU completion for all operations recorded before this call. Submission is
// requested before return; waiting on the future never requires owner progress
// from the waiting guest thread.
std::shared_future<void> Fence();
uint64_t Frames();
renderer::MemoryStats GetMemoryStats();
}
