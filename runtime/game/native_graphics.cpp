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
extern "C" PPC_FUNC(__imp__sub_8259A308);

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
void Cleared(uint8_t* base,uint32_t selected,uint32_t flags,uint32_t rectangle,uint32_t color,float depth,uint32_t stencil) {
    WithDevice(selected,[&](auto& native){
        if(!flags) return;
        if(!gpu::game_shader::GuestRange(selected,13484)) throw std::out_of_range("native clear device range");
        using Rect=gpu::native_render::DrawState::Scissor;
        auto readRect=[&](uint32_t address){
            if(!gpu::game_shader::GuestRange(address,16)) throw std::out_of_range("native clear rectangle/color");
            return Rect{int32_t(PPC_LOAD_U32(address)),int32_t(PPC_LOAD_U32(address+4)),
                int32_t(PPC_LOAD_U32(address+8)),int32_t(PPC_LOAD_U32(address+12))};
        };
        Rect rect;
        if(rectangle) rect=readRect(rectangle);
        else {
            uint32_t first=PPC_LOAD_U32(selected+12536);if(!first) first=PPC_LOAD_U32(selected+12552);
            if(!first) return;
            const auto target=native.DescribeSurface(first);rect={0,0,int32_t(target.width),int32_t(target.height)};
            // Original ClearF uses the logical frame extent when every live
            // attachment is a primary-frame attachment (verified USA fields).
            bool frame=(PPC_LOAD_U8(selected+10432)&16)!=0;
            for(unsigned i=0;i<5&&frame;++i) {
                uint32_t bound=PPC_LOAD_U32(selected+12536+i*4);
                frame=!bound||bound==PPC_LOAD_U32(selected+12972+i*4);
            }
            if(frame) {rect.x1=int32_t(PPC_LOAD_U32(selected+13476));rect.y1=int32_t(PPC_LOAD_U32(selected+13480));}
        }
        const auto viewport=readRect(selected+12808); // x, y, width, height
        int64_t x0=std::max<int64_t>(rect.x0,viewport.x0),y0=std::max<int64_t>(rect.y0,viewport.y0);
        int64_t x1=std::min<int64_t>(rect.x1,int64_t(viewport.x0)+viewport.x1);
        int64_t y1=std::min<int64_t>(rect.y1,int64_t(viewport.y0)+viewport.y1);
        if(PPC_LOAD_U32(selected+11952)) {
            const auto scissor=readRect(selected+12832);
            x0=std::max<int64_t>(x0,scissor.x0);y0=std::max<int64_t>(y0,scissor.y0);
            x1=std::min<int64_t>(x1,scissor.x1);y1=std::min<int64_t>(y1,scissor.y1);
        }
        if(x0>=x1||y0>=y1) return;
        std::array<float,4> rgba{};
        if((flags&15)&&color) {
            if(!gpu::game_shader::GuestRange(color,16)) throw std::out_of_range("native clear color range");
            for(unsigned i=0;i<4;++i) rgba[i]=std::bit_cast<float>(uint32_t(PPC_LOAD_U32(color+i*4)));
        }
        native.ClearBound(flags,{int32_t(x0),int32_t(y0),int32_t(x1),int32_t(y1)},color?rgba.data():nullptr,depth,stencil);
    });
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
PPC_FUNC(sub_8259A308) {
    // ClearF(device, flags, optional RECT*, float4*, depth, stencil). r7 is
    // the ABI slot occupied by the floating depth argument; stencil is r8.
    const uint32_t selected=ctx.r3.u32,flags=ctx.r4.u32,rectangle=ctx.r5.u32,color=ctx.r6.u32,stencil=ctx.r8.u32;
    const float depth=float(ctx.f1.f64);
    __imp__sub_8259A308(ctx,base);
    game::native_graphics::Cleared(base,selected,flags,rectangle,color,depth,stencil);
}
