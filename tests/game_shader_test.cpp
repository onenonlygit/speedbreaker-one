// SpeedBreaker One. GPL-3.0-or-later. Synthetic shaders only.
#include <gpu/game_shader.h>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
using namespace gpu;
using namespace gpu::game_shader;
void Put(std::vector<uint8_t>& b,size_t p,uint32_t v) {
    for(unsigned i=0;i<4;++i)b[p+i]=uint8_t(v>>(24-i*8));
}
int main(int argc,char** argv) {
    std::vector<uint32_t> host={1u|(2u<<12)|(1u<<16),2u<<12,0,
        1u<<12,0x688u|(38u<<16),4u,
        62u|(1u<<15)|(15u<<16)|(50u<<26),0,(2u<<24)|(1u<<31)|(1u<<30)|(1u<<16)|(1u<<8)};
    std::vector<uint32_t> words(host.size());
    for(size_t i=0;i<host.size();++i)words[i]=__builtin_bswap32(host[i]);
    std::vector<uint8_t> blob(64+words.size()*4);
    Put(blob,0,0x102A0E01);Put(blob,4,64);Put(blob,8,words.size()*4);Put(blob,20,24);
    memcpy(blob.data()+64,words.data(),words.size()*4);
    auto original=std::make_shared<Container>();
    assert(ReadContainer(blob,*original) && original->stage==ShaderStage::Vertex);
    assert(original->physicalOffset==64 && original->physicalBytes==36);
    Container invalid;
    for(size_t length=0;length<blob.size();++length)assert(!ReadContainer({blob.data(),length},invalid));
    for(auto field:{0u,4u,8u,20u}) {
        auto bad=blob;Put(bad,field,UINT32_MAX);assert(!ReadContainer(bad,invalid));
    }
    auto bad=blob;Put(bad,0,0x102A1101);assert(!ReadContainer(bad,invalid));
    bad=blob;Put(bad,20,25);assert(!ReadContainer(bad,invalid));
    assert(GuestRange(UINT32_MAX,1) && !GuestRange(UINT32_MAX,2));
    assert(!GuestRange(0,24) && !GuestRange(1,UINT64_MAX));
    Library library;
    library.Remember(0x1000,ShaderStage::Vertex,original);
    auto first=library.Create(0x1000,ShaderStage::Vertex,original,words);
    assert(first && first->translated.ok && first->translated.nativeVertexEligible);
    const auto direct=TranslateShader(ShaderStage::Vertex,host.data(),host.size());
    assert(first->translated.glsl==direct.glsl);
    assert(!library.Create(0x1000,ShaderStage::Pixel,original,words));
    auto changed=words;changed[5]=__builtin_bswap32(8u);
    auto second=library.Create(0x1000,ShaderStage::Vertex,original,changed);
    assert(second && second->id!=first->id);
    assert(second->translated.nativeAttributes[0].stride==32);
    assert(first->translated.nativeAttributes[0].stride==16);
    assert(!library.Find(0,Hash(0,words),changed)); // forced hash collision
    assert(!library.Find(1,Hash(0,words),words));
    assert(!library.Find(0,Hash(0,words),{words.data(),words.size()-1}));
    std::thread producer([&] {for(int i=0;i<200;++i)
        assert(library.Create(0x1000,ShaderStage::Vertex,original,changed)==second);});
    std::thread consumer([&] {for(int i=0;i<200;++i)
        assert(library.Find(0,Hash(0,words),words)==first);});
    producer.join();consumer.join();
    std::fill(words.begin(),words.end(),0);std::fill(blob.begin(),blob.end(),0);
    assert(first->guestWords[0]!=0 && first->original->bytes[0]==0x10);
    library.Remember(0x1000,ShaderStage::Vertex,nullptr);
    assert(!library.Original(0x1000,ShaderStage::Vertex));
    assert(first->translated.glsl==direct.glsl);
    Library bounded(1);
    assert(!bounded.Create(1,ShaderStage::Vertex,original,first->guestWords));
    assert(!bounded.Create(1,ShaderStage::Vertex,original,first->guestWords));
    assert(bounded.GetStats().bytes==0 && bounded.GetStats().rejected==2);
    assert(library.GetStats().created==2 && library.GetStats().hits==200);
    if(argc==2) {
        std::ofstream out(argv[1]);
        const auto& glsl=first->translated.glsl;const auto at=glsl.find('\n')+1;
        out<<glsl.substr(0,at)<<"#define VERTEX_PRIMITIVE_MODE 0\n#define NATIVE_VERTEX_INPUT\n#define POSITION_ONLY\n"<<glsl.substr(at);
        assert(out.good());
    }
    std::cout<<"original containers, exact variants, endian translation, ownership, concurrent creation/lookup and budget passed\n";
}
