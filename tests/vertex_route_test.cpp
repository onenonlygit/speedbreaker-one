// SpeedBreaker One. GPL-3.0-or-later. Synthetic addresses only.
#include <gpu/vertex_route.h>
#include <array>
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>
using namespace gpu::vertex_route;
struct Source { bool ring; uint32_t byte; bool operator==(const Source&) const = default; };
static Source Reference(uint32_t address, const Snapshot* snapshots, unsigned count, uint32_t origin) {
    uint32_t a=address & 0x1FFFFFFFu, relative=0;
    for(unsigned i=0;i<count;++i) {
        auto s=snapshots[i];
        if(a-s.base<s.size) return {true,origin+relative+(a-s.base)};
        relative+=(s.size+255)&~255u;
    }
    return {false,a};
}
static void Check(uint32_t base,uint32_t size,const Snapshot* copies,unsigned count,uint32_t address,unsigned words,uint32_t origin) {
    uint32_t route=Build(base,size,copies,count), a=address&0x1FFFFFFFu,d=a-base;
    bool fast=route!=kFallback && d<=size && words*4<=size-d;
    for(unsigned i=0;i<words;++i) {
        Source actual=fast ? Source{route!=kShared,route==kShared ? a+i*4 : origin+route+a+i*4} : Reference(address+i*4,copies,count,origin);
        assert(actual==Reference(address+i*4,copies,count,origin));
    }
}
static void Dump(const char* file) {
    std::ofstream out(file);assert(out.good());
    const Snapshot copies[]={{0x1000,32},{0x2000,128},{0x1030,64},{0x1FFFFFC0,64}};
    const Snapshot cases[]={{0x1000,32},{0x2004,96},{0x1000,128},{0x1030,64},{0x800,32},{0x1FFFFFC0,64},{0x1000,0},{0x1010,32},{0x1FFFFFFC,4},{0,16},{0x2020,16}};
    out << "#version 460\nlayout(local_size_x=64) in;\nlayout(std430,set=0,binding=0) buffer Output { uint result[]; };\nlayout(push_constant) uniform Seed { uint seed; } test;\n";
    out << "struct Redirects { uvec4 vfRedirect[4]; }; Redirects pc;\nconst uint g_ring[256]=uint[](";
    for(unsigned i=0;i<256;++i)out << (i?",":"") << ((i*1103515245u)^0x98765432u) << 'u';
    out << ");\nuint memLoad(uint w) { return w*1664525u+1013904223u; }\nuint vmem(uint address) { uint a=address&0x1FFFFFFFu;\n";
    for(unsigned i=0;i<4;++i)out << "{uint d=a-pc.vfRedirect["<<i<<"].x;if(d<pc.vfRedirect["<<i<<"].y)return g_ring[(pc.vfRedirect["<<i<<"].z+d)>>2];}\n";
    out << "return memLoad(a>>2);}\n" << Glsl();
    out << "const uvec3 ranges[" << std::size(cases) << "]=uvec3[](";
    for(unsigned i=0;i<std::size(cases);++i)out << (i?",":"") << "uvec3("<<cases[i].base<<"u,"<<cases[i].size<<"u,"<<Build(cases[i].base,cases[i].size,copies,4)<<"u)";
    out << ");\nvoid main(){uint id=gl_GlobalInvocationID.x;if(id>=257)return;\n";
    for(unsigned i=0;i<4;++i)out << "pc.vfRedirect["<<i<<"]=uvec4("<<copies[i].base<<"u,"<<copies[i].size<<"u,"<<i*256<<"u,0u);\n";
    out << "uvec3 r=ranges[id%" << std::size(cases) << "u];uint n=id%4u+1u;uint a=r.x+((id/4u)%9u)*4u-8u; if((id&1u)!=0)a|=0xE0000000u;\nuint vf1=(r.y/4u)<<2;uvec4 got;\n";
    for(unsigned n=1;n<=4;++n)out << (n==1?"if":"else if")<<"(n=="<<n<<"u)got=vertexWords"<<n<<"(a,r.x,vf1,r.z);\n";
    out << "bool ok=true;for(uint j=0;j<n;++j)ok=ok && got[j]==vmem(a+j*4u);uint expected=(id*1664525u+1013904223u)^test.seed;result[id]=ok?expected:(expected^1u);}\n";
}
int main(int argc,char** argv) {
    Snapshot overlap[]={{0x1000,32},{0x1000,128}};
    assert(Build(0x1000,128,overlap,2)==kFallback); // earlier partial wins
    assert(Build(0x1000,16,overlap,2)==uint32_t(0-0x1000));
    assert(Build(0x100,16,overlap,2)==kShared);
    assert(Size(0x1FFFFFFC,0xFFFFFFFC)==4 && Base(0xFFFFFFFF)==0x1FFFFFFC);
    std::mt19937 rng(0x12345678);
    uint64_t comparisons=0;
    for(unsigned iteration=0;iteration<120000;++iteration) {
        std::array<Snapshot,4> copies{};unsigned count=rng()%5;
        uint32_t region=(rng()&0x1FFFu)*4u;
        if(iteration%11==0) region=0x1FFFFE00;
        for(auto& s:copies){s.base=region+(rng()%32)*4;s.size=(rng()%32)*4;s.size=std::min(s.size,kSharedSize-s.base);}
        uint32_t base=region+(rng()%48)*4,size=std::min<uint32_t>((rng()%48)*4u,kSharedSize-base);
        uint32_t origin=(rng()%0x40000)*256u;
        for(unsigned n=1;n<=4;++n)
            for(uint32_t address:{base,base-4,base+size-4,base+size,0xFFFFFFFCu,uint32_t(rng()),base|0xE0000000u}) {
                Check(base,size,copies.data(),count,address&~3u,n,origin);comparisons+=n;
            }
    }
    if(argc==2)Dump(argv[1]);
    std::cout << "Vertex route source/address parity: " << comparisons << " words passed\n";
}
