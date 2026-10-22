// SpeedBreaker One. GPL-3.0-or-later.
#include "game_shader.h"
#include <algorithm>
#include <atomic>
#include <cstdlib>

namespace gpu::game_shader {
namespace {
std::atomic<bool> ready{false};
#if defined(__ANDROID__) && defined(SB_PLUME_RHI)
std::atomic<bool> configured{true};
#else
std::atomic<bool> configured{false};
#endif
uint32_t Be32(std::span<const uint8_t> b, size_t p) {
    return (uint32_t(b[p]) << 24) | (uint32_t(b[p+1]) << 16) | (uint32_t(b[p+2]) << 8) | b[p+3];
}
bool Same(const Resource& r, uint32_t stage, std::span<const uint32_t> words) {
    return uint32_t(r.stage) == stage && r.guestWords.size() == words.size() &&
        std::equal(words.begin(), words.end(), r.guestWords.begin());
}
}
bool GuestRange(uint32_t address, uint64_t size) {
    return address != 0 && size != 0 && size <= (uint64_t(1) << 32) - address;
}
bool ReadContainer(std::span<const uint8_t> bytes, Container& out) {
    out = {};
    if (bytes.size() < 24 || (Be32(bytes,0)&~1u)!=0x102A0E00u) return false;
    // Most Wanted's 2005 header differs from the 2008 container. Physical code
    // starts at virtualSize, not at the shader tag or the shader-header offset.
    const uint32_t v=Be32(bytes,4),p=Be32(bytes,8),h=Be32(bytes,20);
    const uint32_t headerBytes=(Be32(bytes,0)&1) ? 40 : 32;
    if(v<24 || v%4 || !p || p%12 || p>256*1024 || v>1024*1024 ||
        uint64_t(v)+p>bytes.size() || h<24 || h%4 || h>v || headerBytes>v-h) return false;
    out.bytes.assign(bytes.begin(),bytes.begin()+size_t(v)+p);
    out.physicalOffset=v;out.physicalBytes=p;
    out.stage=(Be32(bytes,0)&1) ? ShaderStage::Vertex : ShaderStage::Pixel;
    return true;
}
uint64_t Hash(uint32_t stage,std::span<const uint32_t> words) {
    uint64_t h=0xCBF29CE484222325ull^stage;
    for(uint32_t w:words) h=(h^w)*0x100000001B3ull;
    return h;
}
std::shared_ptr<const Resource> Library::Create(uint32_t object,ShaderStage stage,
    std::shared_ptr<const Container> original,std::span<const uint32_t> words) {
    if(!object || !original || original->stage!=stage || uint32_t(stage)>1 ||
        words.empty() || words.size()>65535 || words.size()*4!=original->physicalBytes) return {};
    const uint64_t hash=Hash(uint32_t(stage),words);
    {
        std::lock_guard lock(mutex_);
        auto [begin,end]=resources_.equal_range(hash);
        for(auto i=begin;i!=end;++i) if(Same(*i->second,uint32_t(stage),words)) {
            ++stats_.reused;return i->second;
        }
        if(saturated_) { ++stats_.rejected;return {}; }
    }
    // Translate immutable copies without holding the library lock. Other guest
    // threads and the CP may look up existing resources while this is running.
    auto r=std::make_shared<Resource>();
    r->stage=stage;r->original=std::move(original);
    r->guestWords.assign(words.begin(),words.end());
    std::vector<uint32_t> host(words.size());
    for(size_t i=0;i<words.size();++i) host[i]=__builtin_bswap32(words[i]);
    r->translated=TranslateShader(stage,host.data(),host.size());
    const size_t cost=sizeof(Resource)+r->guestWords.size()*4+r->translated.glsl.size()+
        r->translated.error.size()+r->translated.nativeAttributes.size()*sizeof(native_vertex::Attribute)+
        r->original->bytes.size();
    std::lock_guard lock(mutex_);
    auto [begin,end]=resources_.equal_range(hash);
    for(auto i=begin;i!=end;++i) if(Same(*i->second,uint32_t(stage),words)) {
        ++stats_.reused;return i->second;
    }
    if(cost>budget_-std::min(budget_,stats_.bytes)) {
        saturated_=true;++stats_.rejected;return {};
    }
    r->id=nextId_++;stats_.bytes+=cost;
    if(r->translated.ok) ++stats_.created;else ++stats_.rejected;
    // Failed translations are also cached to avoid retrying them every draw.
    resources_.emplace(hash,r);return r;
}
void Library::Remember(uint32_t object,ShaderStage stage,std::shared_ptr<const Container> original) {
    if(!object || uint32_t(stage)>1) return;
    std::lock_guard lock(mutex_);
    const uint64_t key=(uint64_t(object)<<1)|uint32_t(stage);
    auto old=objects_.find(key);
    if(old!=objects_.end()) {objectBytes_-=old->second->bytes.size();objects_.erase(old);}
    // Unknown constructors erase any old association at a reused address.
    if(!original || objects_.size()>=8192 || original->bytes.size()>budget_/4-
        std::min(budget_/4,objectBytes_)) return;
    objectBytes_+=original->bytes.size();objects_.emplace(key,std::move(original));
}
std::shared_ptr<const Container> Library::Original(uint32_t object,ShaderStage stage) {
    std::lock_guard lock(mutex_);
    auto i=objects_.find((uint64_t(object)<<1)|uint32_t(stage));
    return i==objects_.end() ? nullptr : i->second;
}
std::shared_ptr<const Resource> Library::Find(uint32_t stage,uint64_t hash,std::span<const uint32_t> words) {
    std::lock_guard lock(mutex_);
    auto [begin,end]=resources_.equal_range(hash);
    for(auto i=begin;i!=end;++i) if(i->second->translated.ok && Same(*i->second,stage,words)) {
        ++stats_.hits;return i->second;
    }
    return {};
}
Library::Stats Library::GetStats() {std::lock_guard lock(mutex_);return stats_;}
Library& GetLibrary() {static Library library;return library;}
bool Enabled() {
    return configured.load(std::memory_order_acquire);
}
void PublishTranslationReady(bool enabled) {
    configured.store(enabled,std::memory_order_release);
    ready.store(true,std::memory_order_release);
}
bool TranslationReady() {return ready.load(std::memory_order_acquire);}
}
