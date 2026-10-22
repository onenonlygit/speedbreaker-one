// Real resource wrappers with synthetic original callees; no original game bodies.
#include <stdafx.h>
#include <game/native_graphics.h>
#include <gpu/native_device.h>

PPC_FUNC(sub_82589FC8);
PPC_FUNC(sub_8258D920);
PPC_FUNC(sub_8258DC88);
PPC_FUNC(sub_8262E9F0);
PPC_FUNC(sub_8259A308);
namespace {
using gpu::native_render::Device;
namespace hooks=game::native_graphics;
namespace worker=gpu::native_render::worker;
constexpr uint32_t Guest=0x60000,Color=0x64000,DepthObject=0x64100,Other=0x64200;
uint32_t nextObject=Color,nextInfo=0;
unsigned calls[5]{};
bool checkRetired=false;
void Result(PPCContext& ctx,uint64_t value) {
    ctx.r3.u64=value;ctx.r4.u64=0xfeed1234abcddcbaull;
    ctx.r11.u64^=0x123456789abcdef0ull;ctx.f1.f64=3.25;ctx.lr^=0x11223344;
}
void Invoke(PPCFunc* function,PPCContext& ctx,uint64_t result) {
    PPCContext expected{};memcpy(&expected,&ctx,sizeof(ctx));Result(expected,result);
    function(ctx,g_memory.base);assert(!memcmp(&ctx,&expected,sizeof(ctx)));
}
uint32_t Create(uint32_t object,uint32_t format=6,uint32_t samples=0,uint32_t width=16,uint32_t height=16) {
    nextObject=object;PPCContext ctx{};ctx.r3.u64=width;ctx.r4.u64=height;ctx.r5.u64=format;ctx.r6.u64=samples;
    auto before=calls[0];Invoke(sub_82589FC8,ctx,object);assert(calls[0]==before+1);return ctx.r3.u32;
}
void Free(uint32_t object) {
    PPCContext ctx{};ctx.r3.u64=object;ctx.r4.u64=0x80000000;
    auto before=calls[3];Invoke(sub_8262E9F0,ctx,0x8899);assert(calls[3]==before+1);
}
void ClearCall(uint32_t device,uint32_t flags,uint32_t rect,uint32_t rgba,float depth,uint32_t stencil) {
    PPCContext ctx{};ctx.r3.u64=device;ctx.r4.u64=flags;ctx.r5.u64=rect;ctx.r6.u64=rgba;
    ctx.r7.u64=0xbadbad;ctx.r8.u64=stencil;ctx.f1.f64=depth;
    auto before=calls[4];Invoke(sub_8259A308,ctx,0x7788);assert(calls[4]==before+1);
}
}
extern "C" PPC_FUNC(__imp__sub_82589FC8) {
    ++calls[0];if(nextObject) {
        memset(base+nextObject,0,64);PPC_STORE_U32(nextObject+52,nextInfo);
        PPC_STORE_U32(nextObject+48,(ctx.r3.u32<<2)|ctx.r6.u32);
    }
    Result(ctx,nextObject);
}
extern "C" PPC_FUNC(__imp__sub_8258D920) {
    ++calls[1];assert(ctx.r4.u32<4);PPC_STORE_U32(ctx.r3.u32+12536+ctx.r4.u32*4,ctx.r5.u32);Result(ctx,0x3344);
}
extern "C" PPC_FUNC(__imp__sub_8258DC88) {
    ++calls[2];PPC_STORE_U32(ctx.r3.u32+12552,ctx.r4.u32);Result(ctx,0x5566);
}
extern "C" PPC_FUNC(__imp__sub_8262E9F0) {
    ++calls[3];if(checkRetired) assert(worker::GetMemoryStats().targetCount==0);
    if(ctx.r3.u32) memset(base+ctx.r3.u32,0xdd,64);Result(ctx,0x8899);
}
extern "C" PPC_FUNC(__imp__sub_8259A308) {++calls[4];Result(ctx,0x7788);}

void TestNativeSurfaceHooks() {
    Device device;assert(!hooks::Attached());
    // Disabled path preserves originals, including unsupported-native MSAA.
    Create(Other,6,2);Free(Other);ClearCall(Other,UINT32_MAX,0,0,3,0);
    hooks::Attach(Guest,device);assert(hooks::Attached());
    bool rejected=false;try {hooks::Attach(Guest,device);} catch(const std::logic_error&) {rejected=true;}assert(rejected);
    assert(Create(0)==0); // allocation failure must not create a resource
    nextInfo=0x321;Create(Color);
    auto first=device.DescribeSurface(Color);
    assert(first.width==16&&first.height==16&&first.format==0&&!first.depth);
    PPCContext color{};color.r3.u64=Guest;color.r4.u64=0;color.r5.u64=Color;
    auto before=calls[1];Invoke(sub_8258D920,color,0x3344);assert(calls[1]==before+1);
    // Another CPU D3D device does not change this device's native bindings.
    PPCContext unrelated{};unrelated.r3.u64=Guest+0x10000;unrelated.r4.u64=0;unrelated.r5.u64=0xabcdef;
    Invoke(sub_8258D920,unrelated,0x3344);
    const float rgba[]={.25f,.5f,.75f,1};device.Clear(Color,0,0,16,16,rgba,1,0);
    auto resource=[](uint32_t id,gpu::ShaderStage stage,const char* glsl){
        auto r=std::make_shared<gpu::game_shader::Resource>();r->id=id;r->stage=stage;r->guestWords={id,0xaaff};
        r->translated.ok=true;r->translated.glsl=glsl;if(stage==gpu::ShaderStage::Pixel) r->translated.colorTargetMask=1;return r;
    };
    device.SetShaders(resource(100,gpu::ShaderStage::Vertex,R"(#version 450
void main(){vec2 p=vec2((gl_VertexIndex<<1)&2,gl_VertexIndex&2);gl_Position=vec4(p*2-1,.25,1);})"),
        resource(101,gpu::ShaderStage::Pixel,R"(#version 450
layout(location=0) out vec4 color;void main(){color=vec4(.25,.5,.75,1);})"));
    auto* base=g_memory.base;
    auto field=[&](uint32_t reg,uint32_t value){PPC_STORE_U32(Guest+gpu::native_render::DrawState::MirrorOffset(reg),value);};
    using namespace xe::gpu;
    field(XE_GPU_REG_RB_MODECONTROL,4);field(XE_GPU_REG_RB_COLOR_MASK,15);field(XE_GPU_REG_RB_BLENDCONTROL0,0x10001);
    field(XE_GPU_REG_PA_CL_VTE_CNTL,0x3f);field(XE_GPU_REG_PA_CL_VPORT_XSCALE,std::bit_cast<uint32_t>(8.f));
    field(XE_GPU_REG_PA_CL_VPORT_YSCALE,std::bit_cast<uint32_t>(8.f));field(XE_GPU_REG_PA_CL_VPORT_XOFFSET,std::bit_cast<uint32_t>(8.f));
    field(XE_GPU_REG_PA_CL_VPORT_YOFFSET,std::bit_cast<uint32_t>(8.f));field(XE_GPU_REG_PA_CL_VPORT_ZSCALE,std::bit_cast<uint32_t>(1.f));
    device.SetScissor({0,0,16,16});
    const float black[]={0,0,0,1};device.Clear(Color,0,0,16,16,black,1,0);
    device.DrawVertices(Guest,[&](uint32_t address){return uint32_t(PPC_LOAD_U32(address));},4,0,3);
    auto pixels=device.Readback(Color,0,0,16,16);
    for(unsigned i=0;i<256;++i) {assert(pixels[i*4]==64&&pixels[i*4+1]==128&&pixels[i*4+2]==191&&pixels[i*4+3]==255);}
    // Original ClearF's r5 is one optional RECT*, not a rectangle count.
    constexpr uint32_t Rectangle=0x65000,Rgba=0x65020;
    auto rect=[&](uint32_t address,int32_t x0,int32_t y0,int32_t x1,int32_t y1){
        PPC_STORE_U32(address,uint32_t(x0));PPC_STORE_U32(address+4,uint32_t(y0));
        PPC_STORE_U32(address+8,uint32_t(x1));PPC_STORE_U32(address+12,uint32_t(y1));
    };
    auto colorValue=[&](float r,float g,float b,float a){const float values[]={r,g,b,a};
        for(unsigned i=0;i<4;++i) {PPC_STORE_U32(Rgba+i*4,std::bit_cast<uint32_t>(values[i]));}
    };
    rect(Guest+12808,2,1,10,12); // viewport x/y/width/height
    PPC_STORE_U32(Guest+11952,1);rect(Guest+12832,4,3,9,11);
    rect(Rectangle,0,2,14,8);colorValue(0,.5f,0,1);
    ClearCall(Guest,1,Rectangle,Rgba,.25f,0x123);
    auto clipped=device.Readback(Color,0,0,16,16);
    for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x) {
        bool inside=x>=4&&x<9&&y>=3&&y<8;auto at=(y*16+x)*4;
        assert(clipped[at]==(inside?0:64)&&clipped[at+1]==128&&clipped[at+2]==(inside?0:191)&&clipped[at+3]==255);
    }
    colorValue(0,0,1,1);ClearCall(Guest,1,0,Rgba,1,0);
    clipped=device.Readback(Color,0,0,16,16);
    for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x) {
        bool inside=x>=4&&x<9&&y>=3&&y<11;auto at=(y*16+x)*4;
        assert(clipped[at]==(inside?0:64)&&clipped[at+1]==(inside?0:128)&&clipped[at+2]==(inside?255:191)&&clipped[at+3]==255);
    }
    ClearCall(Guest+0x10000,1,0,Rgba,1,0);assert(device.Readback(Color,0,0,16,16)==clipped);
    // Default primary-frame rectangle uses the logical frame extents.
    PPC_STORE_U8(Guest+10432,16);PPC_STORE_U32(Guest+12972,Color);
    PPC_STORE_U32(Guest+13476,6);PPC_STORE_U32(Guest+13480,7);
    rect(Guest+12808,0,0,16,16);PPC_STORE_U32(Guest+11952,0);
    colorValue(1,0,0,1);ClearCall(Guest,1,0,Rgba,1,0);
    auto framed=device.Readback(Color,0,0,16,16);
    for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x) {
        auto at=(y*16+x)*4;if(x<6&&y<7) {assert(framed[at]==255&&framed[at+1]==0&&framed[at+2]==0&&framed[at+3]==255);}
        else assert(!memcmp(framed.data()+at,clipped.data()+at,4));
    }
    PPC_STORE_U8(Guest+10432,0);
    // Reject unsupported color conversion before any target of a multi-clear changes.
    nextInfo=1u<<16;Create(Other);
    PPCContext extra{};extra.r3.u64=Guest;extra.r4.u64=1;extra.r5.u64=Other;
    Invoke(sub_8258D920,extra,0x3344);
    rejected=false;try {ClearCall(Guest,3,0,Rgba,1,0);} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    assert(device.Readback(Color,0,0,16,16)==framed);
    extra={};extra.r3.u64=Guest;extra.r4.u64=1;extra.r5.u64=0;Invoke(sub_8258D920,extra,0x3344);Free(Other);
    nextInfo=0x321;
    // Reused object address retires the old allocation; actual dimensions win.
    Create(Color,6,0,18,20);auto second=device.DescribeSurface(Color);assert(second.handle!=first.handle);
    assert(second.width==18&&second.height==20);assert(worker::GetMemoryStats().targetCount==0);
    nextInfo=(1u<<16)|0x345;Create(DepthObject,23);auto depth=device.DescribeSurface(DepthObject);
    assert(depth.depth&&depth.format==1);
    PPCContext ds{};ds.r3.u64=Guest;ds.r4.u64=DepthObject;
    before=calls[2];Invoke(sub_8258DC88,ds,0x5566);assert(calls[2]==before+1);
    device.Clear(DepthObject,0,0,16,16,nullptr,.75f,23);
    rect(Rectangle,1,1,5,5);
    ClearCall(Guest,4|16,Rectangle,0,.9f,777); // selected color slot 2 is unbound: no clear draw
    auto unchangedDepth=device.Readback(DepthObject,0,0,16,16);
    for(unsigned i=0;i<256;++i) {float value;memcpy(&value,unchangedDepth.data()+i*4,4);assert(value==.75f);}
    ClearCall(Guest,16,Rectangle,0,.25f,99);
    ClearCall(Guest,32,Rectangle,0,std::numeric_limits<float>::quiet_NaN(),99);
    auto z=device.Readback(DepthObject,0,0,16,16);assert(z.size()==1024);
    auto stencil=device.Readback(DepthObject,0,0,16,16,true);
    for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x) {
        auto i=y*16+x;bool inside=x>=1&&x<5&&y>=1&&y<5;
        float value;memcpy(&value,z.data()+i*4,4);assert(value==(inside?.25f:.75f));assert(stencil[i]==(inside?99:23));
    }
    // Free must retire native GPU work before its CPU object can be recycled.
    checkRetired=true;Free(DepthObject);checkRetired=false;
    rejected=false;try {device.DescribeSurface(DepthObject);} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    // Non-surface heap allocations pass through without native effects.
    Free(Other);
    // Multisampling is not silently degraded to a single sample.
    rejected=false;try {Create(Other,6,1);} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    Free(Other);
    device.Clear(Color,0,0,18,20,rgba,1,0);assert(worker::GetMemoryStats().targetCount==1);
    hooks::Detach();assert(!hooks::Attached());assert(worker::GetMemoryStats().targetCount==0);
    rejected=false;try {device.DescribeSurface(Color);} catch(const std::invalid_argument&) {rejected=true;}assert(rejected);
    // Detach restores exact pass-through without retaining a device pointer.
    Create(Other,6,2);Free(Other);
}
