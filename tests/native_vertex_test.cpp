// SpeedBreaker One. GPL-3.0-or-later. Synthetic input only.
#include <gpu/native_vertex.h>
#include <gpu/shader_translator.h>
#include <cassert>
#include <iostream>
#include <random>
using namespace gpu;
int main() {
    const vertex_route::Snapshot copies[]={{0x1000,512},{0x2000,256}};
    uint32_t offset=0;
    native_vertex::Attribute a{0,16,4,0};
    assert(native_vertex::Plan(a,0x1000,512,copies,2,0,31,0,offset) && offset==0);
    assert(!native_vertex::Plan(a,0x1000,512,copies,2,0,32,0,offset));
    assert(native_vertex::Plan(a,0x2000,256,copies,2,1,8,0,offset) && offset==512);
    a.offset=-16;assert(!native_vertex::Plan(a,0x1000,512,copies,2,1,10,0,offset));
    a.offset=0;assert(!native_vertex::Plan(a,0x1000,512,copies,2,0,4,0xFFFFFFFFu,offset));
    bool shared=false;
    assert(native_vertex::Plan(a,0x1800,512,copies,2,0,4,0,offset,&shared) && shared && offset==0x1800);
    const vertex_route::Snapshot partial[]={{0x1000,32},{0x1000,128}};
    assert(!native_vertex::Plan(a,0x1000,128,partial,2,0,1,0,offset));
    std::mt19937 rng(17);uint64_t checked=0;
    for(unsigned k=0;k<200000;++k) {
        a.stride=uint32_t(rng()%8)*4;a.offset=int32_t(rng()%33)*4-32;a.words=1+rng()%4;
        uint32_t first=rng()%10,last=first+rng()%15,add=rng()%8;
        if(!native_vertex::Plan(a,0x1000,512,copies,2,first,last,add,offset))continue;
        for(uint32_t index=first;index<=last;++index)
            for(unsigned word=0;word<a.words;++word) {
                uint32_t shaderAddress=0x1000+(index+add)*a.stride+a.offset+word*4;
                uint32_t hardwareOffset=offset+index*a.stride+word*4;
                assert(shaderAddress>=0x1000 && shaderAddress+4<=0x1200);
                assert(hardwareOffset==shaderAddress-0x1000);++checked;
            }
    }
    // Full fetch r0.x -> r1, position export; no game code.
    std::vector<uint32_t> code={1u|(2u<<12)|(1u<<16),2u<<12,0,
        1u<<12,0x688u|(38u<<16),4u,
        62u|(1u<<15)|(15u<<16)|(50u<<26),0,(2u<<24)|(1u<<31)|(1u<<30)};
    auto sh=TranslateShader(ShaderStage::Vertex,code.data(),code.size());
    assert(sh.ok && sh.nativeVertexEligible && sh.nativeAttributes.size()==1);
    assert(sh.nativeAttributes[0].words==4 && sh.nativeAttributes[0].stride==16);
    code[3]|=1u<<5; // fetch from r1, not the original vertex ID
    sh=TranslateShader(ShaderStage::Vertex,code.data(),code.size());assert(sh.ok && !sh.nativeVertexEligible);
    code[3]=0; // last fetch may overwrite r0: no later fetch depends on it
    sh=TranslateShader(ShaderStage::Vertex,code.data(),code.size());assert(sh.ok && sh.nativeVertexEligible);
    code[0]=1u|(3u<<12)|(5u<<16);
    code.insert(code.begin()+6,{1u<<12,0x688u|(38u<<16),4u});
    sh=TranslateShader(ShaderStage::Vertex,code.data(),code.size());assert(sh.ok && !sh.nativeVertexEligible);
    std::cout<<"Native vertex address/format/eligibility checks passed: "<<checked<<" words\n";
}
