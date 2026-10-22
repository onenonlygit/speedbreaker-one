// SpeedBreaker One. GPL-3.0-or-later. Synthetic CPU driver object only.
#include <gpu/native_draw_state.h>
#include <cassert>
#include <iostream>
#include <limits>
using namespace gpu::native_render;
using namespace xe::gpu;
template<class F> void Reject(F&& f) { bool rejected=false;try { f(); } catch(const std::out_of_range&) { rejected=true; } assert(rejected); }
int main() {
    constexpr uint32_t device=0x82001000;
    std::array<uint32_t,0x2EB0/4> words{};
    // A different value at every address catches an incorrect mirror base,
    // byte/word offset, VS/PS constant alias or host/guest endian assumption.
    for (uint32_t i=0;i<words.size();++i) words[i]=0xA5100000u+i;
    auto read=[&](uint32_t address) { assert(address>=device && address-device<sizeof(words));assert((address&3)==0);return words[(address-device)/4]; };
    auto state=DrawState::Capture(device,read);
    assert(state.rb_surface_info==read(device+0x2CC0));
    assert(state.rb_depthcontrol==read(device+0x2D74));
    assert(state.vgt_indx_offset==read(device+0x2D14));
    assert(state.pa_cl_vport_xscale==read(device+0x2D48));
    assert(state.pa_su_poly_offset_front_scale==read(device+0x2E90));
    assert(state.pa_sc_screen_scissor_tl==read(device+0x2CF8));
    assert(state.pa_sc_window_offset==0 && state.pa_sc_window_scissor_br==0);
    assert(state.floats.front()==read(device+0x780));
    assert(state.floats[1024]==read(device+0x1780));
    assert(state.floats.back()==read(device+0x277C));
    assert(state.fetch.front()==read(device+0x480) && state.fetch.back()==read(device+0x77C));
    assert(state.bools.front()==read(device+0x2780) && state.bools.back()==read(device+0x279C));
    assert(state.loops.front()==read(device+0x27A0) && state.loops.back()==read(device+0x281C));
    assert(state.Word(XE_GPU_REG_RB_DEPTHCONTROL)==state.rb_depthcontrol);
    assert(state.Words(XE_GPU_REG_SHADER_CONSTANT_000_X,2048)==state.floats.data());
    assert(state.Words(XE_GPU_REG_SHADER_CONSTANT_FETCH_00_0,192)==state.fetch.data());
    assert(state.Word(XE_GPU_REG_SHADER_CONSTANT_BOOL_000_031)==state.bools[0]);
    Reject([&]{state.Words(XE_GPU_REG_SHADER_CONSTANT_000_X+2047,2);});
    Reject([&]{state.Words(XE_GPU_REG_SHADER_CONSTANT_FETCH_00_0+191,2);});
    Reject([&]{state.Words(XE_GPU_REG_SHADER_CONSTANT_LOOP_00+31,2);});
    Reject([&]{state.Words(XE_GPU_REG_SHADER_CONSTANT_LOOP_00,std::numeric_limits<uint32_t>::max());});
    Reject([&]{state.Word(XE_GPU_REG_RB_COPY_CONTROL);});
    Reject([&]{DrawState::MirrorOffset(XE_GPU_REG_PA_SC_WINDOW_OFFSET);});
    Reject([&]{DrawState::Capture(0xFFFFFFF0,read);});
    auto old=state.floats[0];words[0x780/4]=0;assert(state.floats[0]==old);
    auto copy=state;state.fetch[0]=0;assert(copy.fetch[0]!=0);
    std::cout<<"Native CPU state ABI, immutable snapshots and bounds passed\n";
}
