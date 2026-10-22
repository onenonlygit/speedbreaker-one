// SpeedBreaker One. GPL-3.0-or-later. Uses the actual generated PPC context,
// with synthetic constructor/patcher callees. No game function bodies linked.
#include <stdafx.h>
#include <gpu/game_shader.h>
#include <cassert>
#include <iostream>
PPC_FUNC(sub_8259BA98);
PPC_FUNC(sub_8259BE40);
PPC_FUNC(sub_825A2DC0);
namespace {
std::array<uint8_t,sizeof(PPCContext)> expected;
uint32_t calls=0;
void Store(uint8_t* base,uint32_t at,uint32_t value) {
    value=__builtin_bswap32(value);std::memcpy(base+at,&value,4);
}
void Returned(PPCContext& ctx) {
    ++calls;ctx.r4.u64=0xDEADBEEF12345678ull;ctx.r5.u64=0;
    ctx.lr=0x87654321;ctx.r12.u64=0x0123456789ABCDEFull;
    std::memcpy(expected.data(),&ctx,sizeof(ctx));
}
void Construct(PPCContext& ctx,uint8_t* base,uint32_t offset) {
    const uint32_t input=ctx.r3.u32;
    const uint32_t v=PPC_LOAD_U32(input+4),p=PPC_LOAD_U32(input+8);
    std::memcpy(base+0x10000,base+input+v,p);
    Store(base,0x8000+offset,0x10000);ctx.r3.u64=0x8000;Returned(ctx);
}
}
extern "C" PPC_FUNC(__imp__sub_8259BA98) {Construct(ctx,base,12);}
extern "C" PPC_FUNC(__imp__sub_8259BE40) {Construct(ctx,base,40);}
extern "C" PPC_FUNC(__imp__sub_825A2DC0) {
    std::memcpy(base+ctx.r5.u32,base+0x10000,36);
    Store(base,ctx.r5.u32+20,8);Returned(ctx);
}
int main() {
    using namespace gpu;using namespace game_shader;
    std::vector<uint8_t> memory(0x20000);uint8_t* base=memory.data();
    constexpr uint32_t input=0x1000;
    Store(base,input,0x102A0E01);Store(base,input+4,64);Store(base,input+8,36);Store(base,input+20,24);
    const uint32_t code[]={1u|(2u<<12)|(1u<<16),2u<<12,0,1u<<12,0x688u|(38u<<16),4u,
        62u|(1u<<15)|(15u<<16)|(50u<<26),0,(2u<<24)|(1u<<31)|(1u<<30)|(1u<<16)|(1u<<8)};
    for(unsigned i=0;i<9;++i)Store(base,input+64+i*4,code[i]);
    PublishTranslationReady(true);
    PPCContext ctx{};ctx.r3.u64=input;sub_8259BE40(ctx,base);
    assert(calls==1 && !std::memcmp(&ctx,expected.data(),sizeof(ctx)));
    auto original=GetLibrary().Original(0x8000,ShaderStage::Vertex);
    assert(original && original->physicalBytes==36);
    std::array<uint32_t,9> words;std::memcpy(words.data(),base+0x10000,36);
    auto first=GetLibrary().Find(0,Hash(0,words),words);
    assert(first && first->translated.nativeAttributes[0].stride==16);
    ctx.r4.u64=0x8000;ctx.r5.u64=0x11000;sub_825A2DC0(ctx,base);
    assert(calls==2 && !std::memcmp(&ctx,expected.data(),sizeof(ctx)));
    std::memcpy(words.data(),base+0x11000,36);
    auto variant=GetLibrary().Find(0,Hash(0,words),words);
    assert(variant && variant->id!=first->id && variant->translated.nativeAttributes[0].stride==32);
    assert(first->translated.nativeAttributes[0].stride==16);
    // Reuse by an unrecognized container invalidates the previous identity.
    Store(base,input,0x102A1101);ctx.r3.u64=input;sub_8259BE40(ctx,base);
    assert(calls==3 && !std::memcmp(&ctx,expected.data(),sizeof(ctx)));
    assert(!GetLibrary().Original(0x8000,ShaderStage::Vertex));
    // Disabled capture still calls the guest and preserves every context byte.
    PublishTranslationReady(false);Store(base,input,0x102A0E00);ctx.r3.u64=input;
    sub_8259BA98(ctx,base);
    assert(calls==4 && !std::memcmp(&ctx,expected.data(),sizeof(ctx)));
    std::cout<<"actual PPC context, constructor and patcher ABI preservation, variants, address reuse and disabled fallback passed\n";
}
