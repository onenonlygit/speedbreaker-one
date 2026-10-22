#version 460
layout(location=0) out vec4 color;
layout(push_constant) uniform P { vec4 value; } p;
void main() { color=p.value; }
