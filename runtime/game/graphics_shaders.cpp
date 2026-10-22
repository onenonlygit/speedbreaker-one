// SpeedBreaker One. GPL-3.0-or-later.
// USA executable: constructors and declaration fetch patcher verified against
// generated bodies. Original PPC retains allocation, refcounts and bookkeeping.
#include <stdafx.h>
#include <kernel/function.h>
#include <gpu/game_shader.h>

extern "C" PPC_FUNC(__imp__sub_8259BA98);
extern "C" PPC_FUNC(__imp__sub_8259BE40);
extern "C" PPC_FUNC(__imp__sub_825A2DC0);
namespace {
using namespace gpu::game_shader;
using gpu::ShaderStage;
std::shared_ptr<const Container> Capture(uint8_t* base,uint32_t address,ShaderStage stage) {
    if(!Enabled() || !GuestRange(address,24)) return {};
    if(PPC_LOAD_U32(address)!=(stage==ShaderStage::Vertex ? 0x102A0E01u : 0x102A0E00u)) return {};
    const uint32_t v=PPC_LOAD_U32(address+4),p=PPC_LOAD_U32(address+8);
    const uint64_t size=uint64_t(v)+p;
    if(v>1024*1024 || p>256*1024 || !GuestRange(address,size)) return {};
    auto result=std::make_shared<Container>();
    if(!ReadContainer({base+address,size},*result)) return {};
    return result;
}
void Executable(uint8_t* base,uint32_t object,ShaderStage stage,
    const std::shared_ptr<const Container>& original,uint32_t address) {
    if(!original || !TranslationReady() || !GuestRange(address,original->physicalBytes)) return;
    // Ring copies and in-place patches have different lifetimes. Keep neither
    // guest pointers nor mutable views of either in an asynchronous render job.
    std::vector<uint32_t> words(original->physicalBytes/4);
    memcpy(words.data(),base+address,original->physicalBytes);
    GetLibrary().Create(object,stage,original,words);
}
void Finish(uint8_t* base,uint32_t object,ShaderStage stage,std::shared_ptr<const Container> original) {
    if(!Enabled() || !object) return;
    GetLibrary().Remember(object,stage,original);
    const uint32_t offset=stage==ShaderStage::Vertex ? 40 : 12;
    if(original && GuestRange(object,uint64_t(offset)+4))
        Executable(base,object,stage,original,PPC_LOAD_U32(object+offset));
}
template<class F> void Optional(F&& f) noexcept {
    try {f();} catch(const std::exception&) {
        static std::atomic<bool> reported{false};
        if(!reported.exchange(true)) fprintf(stderr,"[game-shaders] capture failed; compatibility translation retained\n");
    }
}
}
PPC_FUNC(sub_8259BA98) {
    std::shared_ptr<const Container> original;
    Optional([&] {original=Capture(base,ctx.r3.u32,ShaderStage::Pixel);});
    __imp__sub_8259BA98(ctx,base);
    Optional([&] {Finish(base,ctx.r3.u32,ShaderStage::Pixel,std::move(original));});
}
PPC_FUNC(sub_8259BE40) {
    std::shared_ptr<const Container> original;
    Optional([&] {original=Capture(base,ctx.r3.u32,ShaderStage::Vertex);});
    __imp__sub_8259BE40(ctx,base);
    Optional([&] {Finish(base,ctx.r3.u32,ShaderStage::Vertex,std::move(original));});
}
PPC_FUNC(sub_825A2DC0) {
    const uint32_t object=ctx.r4.u32,destination=ctx.r5.u32;
    __imp__sub_825A2DC0(ctx,base);
    if(!Enabled() || !TranslationReady()) return;
    Optional([&] {Executable(base,object,ShaderStage::Vertex,
        GetLibrary().Original(object,ShaderStage::Vertex),destination);});
}
