// SpeedBreaker One. GPL-3.0-or-later.
// Supported USA executable. Original functions retain CPU resource bookkeeping;
// the native attachment forwards their resource/binding effects after completion.
#include <stdafx.h>
#include <kernel/function.h>
#include <gpu/native_device.h>
#include "native_graphics.h"
#include <shared_mutex>

extern "C" PPC_FUNC(__imp__sub_82589FC8);
extern "C" PPC_FUNC(__imp__sub_8258D920);
extern "C" PPC_FUNC(__imp__sub_8258DC88);
extern "C" PPC_FUNC(__imp__sub_8262E9F0);

namespace game::native_graphics {
namespace {
std::shared_mutex mutex;
std::atomic<bool> attached{false};
gpu::native_render::Device* device=nullptr;
uint32_t guest=0;
template<class F> void WithDevice(uint32_t selected,const F& f) {
    if(!attached.load(std::memory_order_acquire)) return;
    std::shared_lock lock(mutex);
    if(device&&(!selected||selected==guest)) f(*device);
}
}
void Attach(uint32_t guestDevice,gpu::native_render::Device& native) {
    if(!gpu::game_shader::GuestRange(guestDevice,0x3400)) throw std::invalid_argument("invalid native guest device");
    if(!gpu::native_render::worker::Running()) throw std::logic_error("native game resources require native worker startup");
    std::unique_lock lock(mutex);
    if(device) throw std::logic_error("native game device already attached");
    guest=guestDevice;device=&native;attached.store(true,std::memory_order_release);
}
void Detach() {
    std::unique_lock lock(mutex);
    if(device) device->Reset();
    device=nullptr;guest=0;attached.store(false,std::memory_order_release);
}
bool Attached() {return attached.load(std::memory_order_acquire);}

void SurfaceCreated(uint8_t* base,uint32_t object,uint32_t width,uint32_t height,uint32_t d3dFormat,uint32_t samples) {
    if(!object) return;
    WithDevice(0,[&](auto& native){
        if(!gpu::game_shader::GuestRange(object,64)) throw std::out_of_range("native surface object range");
        if(samples) throw std::invalid_argument("native multisampled surfaces not implemented");
        const bool depth=(d3dFormat&63)==22||(d3dFormat&63)==23;
        const uint32_t info=PPC_LOAD_U32(object+52);
        // Constructor writes RB_*_INFO into object+52. Keep format, never base.
        uint32_t format=depth ? uint32_t(xe::gpu::reg::RB_DEPTH_INFO{info}.depth_format)
                              : uint32_t(xe::gpu::reg::RB_COLOR_INFO{info}.color_format);
        native.CreateSurface(object,width,height,format,depth);
    });
}
void ColorBound(uint8_t* base,uint32_t selected,uint32_t slot) {
    WithDevice(selected,[&](auto& native){
        if(slot>=4||!gpu::game_shader::GuestRange(selected,12552)) throw std::out_of_range("native color binding");
        // Read the resulting binding, not volatile registers clobbered by PPC.
        native.SetColor(slot,PPC_LOAD_U32(selected+12536+slot*4));
    });
}
void DepthBound(uint8_t* base,uint32_t selected) {
    WithDevice(selected,[&](auto& native){
        if(!gpu::game_shader::GuestRange(selected,12556)) throw std::out_of_range("native depth binding");
        native.SetDepth(PPC_LOAD_U32(selected+12552));
    });
}
void BeforeFree(uint32_t object) {
    if(object) WithDevice(0,[&](auto& native){native.DestroySurface(object);});
}
}

PPC_FUNC(sub_82589FC8) {
    const uint32_t width=ctx.r3.u32,height=ctx.r4.u32,format=ctx.r5.u32,samples=ctx.r6.u32;
    __imp__sub_82589FC8(ctx,base);
    game::native_graphics::SurfaceCreated(base,ctx.r3.u32,width,height,format,samples);
}
PPC_FUNC(sub_8258D920) {
    const uint32_t selected=ctx.r3.u32,slot=ctx.r4.u32;
    __imp__sub_8258D920(ctx,base);
    game::native_graphics::ColorBound(base,selected,slot);
}
PPC_FUNC(sub_8258DC88) {
    const uint32_t selected=ctx.r3.u32;
    __imp__sub_8258DC88(ctx,base);
    game::native_graphics::DepthBound(base,selected);
}
PPC_FUNC(sub_8262E9F0) {
    // Generic game heap free: retire only objects in the native surface map,
    // before the original can recycle their CPU allocation. Other frees pass through.
    game::native_graphics::BeforeFree(ctx.r3.u32);
    __imp__sub_8262E9F0(ctx,base);
}
