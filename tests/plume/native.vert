#version 460
layout(location=0) in uvec4 raw;
const vec2 pos[3]=vec2[](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
uint swap32(uint v){return (v<<24)|((v<<8)&0x00FF0000u)|((v>>8)&0x0000FF00u)|(v>>24);}
void main(){
    float sum=uintBitsToFloat(swap32(raw.x));
#if NATIVE_WORDS >= 2
    sum+=uintBitsToFloat(swap32(raw.y));
#endif
#if NATIVE_WORDS >= 3
    sum+=uintBitsToFloat(swap32(raw.z));
#endif
#if NATIVE_WORDS >= 4
    sum+=uintBitsToFloat(swap32(raw.w));
#endif
    gl_Position=vec4(pos[gl_VertexIndex]*sum/float((1u<<NATIVE_WORDS)-1u),0,1);
}
