// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include "native_worker.h"
#include <atomic>
#include <limits>
#include <mutex>
#include <unordered_map>

namespace gpu::native_render {
// CPU D3D-call bridge. Allocation identity belongs to a game object generation,
// never its EDRAM placement. Guest hook callers must supply verified descriptors
// and hold the game's device lock while Capture reads its CPU state mirrors.
class Device {
public:
    struct IndexBuffer {uint32_t physical=0,bytes=0;bool indices32=false;uint32_t endian=0;};
    TargetDesc DescribeSurface(uint32_t object) {std::lock_guard lock(mutex_);return Surface(object);}
    void Reset() {
        std::lock_guard lock(mutex_);
        for(const auto& [object,surface]:surfaces_) worker::Destroy(surface.handle);
        surfaces_.clear();colors_={};textures_={};depth_=0;
        vertex_.reset();pixel_.reset();scissorSet_=false;
    }
    TargetDesc CreateSurface(uint32_t object,uint32_t width,uint32_t height,uint32_t format,bool depth) {
        if(!object||!width||!height||width>16384||height>16384) throw std::invalid_argument("invalid native surface object/extent");
        std::lock_guard lock(mutex_);
        uint64_t handle=next_.load(std::memory_order_relaxed);
        do {if(handle==UINT64_MAX) throw std::overflow_error("native surface generation exhausted");}
        while(!next_.compare_exchange_weak(handle,handle+1,std::memory_order_relaxed));
        auto old=surfaces_.find(object);
        if(old==surfaces_.end()&&surfaces_.size()>=4096) throw std::length_error("native surface object budget");
        if(old!=surfaces_.end()) {worker::Destroy(old->second.handle);surfaces_.erase(old);}
        // A shared atomic also distinguishes objects belonging to different devices.
        TargetDesc desc{handle,width,height,format,depth};surfaces_.emplace(object,desc);return desc;
    }
    void DestroySurface(uint32_t object) {
        std::lock_guard lock(mutex_);auto i=surfaces_.find(object);if(i==surfaces_.end()) return;
        worker::Destroy(i->second.handle);surfaces_.erase(i);
        for(auto& bound:colors_) if(bound==object) bound=0;
        if(depth_==object) depth_=0;
        for(auto& bound:textures_) if(bound==object) bound=0;
    }
    void SetColor(uint32_t slot,uint32_t object) {
        std::lock_guard lock(mutex_);if(slot>=4) throw std::out_of_range("native color slot");
        if(object&&Surface(object).depth) throw std::invalid_argument("depth surface bound as color");colors_[slot]=object;
    }
    void SetDepth(uint32_t object) {
        std::lock_guard lock(mutex_);if(object&&!Surface(object).depth) throw std::invalid_argument("color surface bound as depth");depth_=object;
    }
    void SetTexture(uint32_t slot,uint32_t object) {
        std::lock_guard lock(mutex_);if(slot>=32) throw std::out_of_range("native texture slot");
        if(object) Surface(object);textures_[slot]=object;
    }
    void SetShaders(std::shared_ptr<const game_shader::Resource> vertex,std::shared_ptr<const game_shader::Resource> pixel) {
        if(!vertex||vertex->stage!=ShaderStage::Vertex||(pixel&&pixel->stage!=ShaderStage::Pixel)) throw std::invalid_argument("native shader stages");
        std::lock_guard lock(mutex_);vertex_=std::move(vertex);pixel_=std::move(pixel);
    }
    void SetScissor(DrawState::Scissor scissor) {
        if(scissor.x1<scissor.x0||scissor.y1<scissor.y0) throw std::invalid_argument("native scissor bounds");
        std::lock_guard lock(mutex_);scissor_=scissor;scissorSet_=true;
    }
    template<class Reader> void DrawVertices(uint32_t guestDevice,const Reader& read,uint32_t primitive,uint32_t first,uint32_t count) {
        std::lock_guard lock(mutex_);auto draw=Capture(guestDevice,read);draw.primitive=primitive;draw.count=count;
        draw.state.vgt_indx_offset=first;worker::Draw(std::move(draw));
    }
    template<class Reader> void DrawIndexed(uint32_t guestDevice,const Reader& read,uint32_t primitive,
        IndexBuffer indices,uint32_t first,uint32_t count,int32_t baseVertex=0) {
        const uint32_t stride=indices.indices32?4:2;
        uint64_t offset=uint64_t(first)*stride,bytes=uint64_t(count)*stride;
        if(indices.endian>3||indices.physical%stride||offset>indices.bytes||bytes>indices.bytes-offset||
            uint64_t(indices.physical)+indices.bytes>512ull*1024*1024) throw std::out_of_range("native index buffer range");
        std::lock_guard lock(mutex_);auto draw=Capture(guestDevice,read);draw.primitive=primitive;draw.count=count;
        draw.indexed=true;draw.indices32=indices.indices32;draw.endian=indices.endian;
        draw.indexBase=indices.physical+uint32_t(offset);draw.state.vgt_indx_offset=uint32_t(baseVertex);worker::Draw(std::move(draw));
    }
    void Clear(uint32_t object,int32_t x,int32_t y,uint32_t w,uint32_t h,const float* color,
        float depth,uint32_t stencil,bool clearDepth=true,bool clearStencil=true) {
        std::lock_guard lock(mutex_);worker::Clear(Surface(object),x,y,w,h,color,depth,stencil,clearDepth,clearStencil);
    }
    void Copy(uint32_t source,uint32_t dest,uint32_t sx,uint32_t sy,uint32_t dx,uint32_t dy,uint32_t w,uint32_t h) {
        std::lock_guard lock(mutex_);worker::Copy(Surface(source),Surface(dest),sx,sy,dx,dy,w,h);
    }
    bool Present(uint32_t object,uint32_t w,uint32_t h) {std::lock_guard lock(mutex_);return worker::Present(Surface(object),w,h);}
    std::vector<uint8_t> Readback(uint32_t object,uint32_t x,uint32_t y,uint32_t w,uint32_t h,bool stencil=false) {
        std::lock_guard lock(mutex_);return worker::Readback(Surface(object),x,y,w,h,stencil);
    }
private:
    TargetDesc Surface(uint32_t object) const {
        auto i=surfaces_.find(object);if(i==surfaces_.end()) throw std::invalid_argument("untracked native surface object");return i->second;
    }
    template<class Reader> native_render::Draw Capture(uint32_t guestDevice,const Reader& read) const {
        if(!scissorSet_) throw std::logic_error("native device scissor has not been supplied");
        native_render::Draw draw;draw.state=DrawState::Capture(guestDevice,read);
        auto& state=draw.state;state.scissor=scissor_;state.vertex=vertex_;state.pixel=pixel_;
        for(unsigned i=0;i<4;++i) if(colors_[i]) state.colors[i]=Surface(colors_[i]);
        if(depth_) state.depth=Surface(depth_);
        for(unsigned i=0;i<32;++i) if(textures_[i]) state.textures[i]=Surface(textures_[i]).handle;
        return draw;
    }
    inline static std::atomic<uint64_t> next_{1ull<<32};
    std::mutex mutex_;
    std::unordered_map<uint32_t,TargetDesc> surfaces_;
    std::array<uint32_t,4> colors_{};
    std::array<uint32_t,32> textures_{};
    uint32_t depth_=0;
    DrawState::Scissor scissor_{};bool scissorSet_=false;
    std::shared_ptr<const game_shader::Resource> vertex_,pixel_;
};
}
