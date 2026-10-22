// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include "game_shader.h"
#include "xenos/registers.h"
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>

namespace gpu::native_render {
using Handle = uint64_t;
struct TargetDesc {
    Handle handle = 0;
    uint32_t width = 0, height = 0, format = 0;
    bool depth = false;
};
// Game driver state, not a GPU register file. Pipeline bits retain the game's
// D3D encoding at this boundary; shader constants/descriptors have their own
// typed storage. There is no PM4, EDRAM allocation or GPU-register mutation API.
struct DrawState {
    uint32_t pa_cl_clip_cntl = 0;
    uint32_t pa_cl_vport_xoffset = 0;
    uint32_t pa_cl_vport_xscale = 0;
    uint32_t pa_cl_vport_yoffset = 0;
    uint32_t pa_cl_vport_yscale = 0;
    uint32_t pa_cl_vport_zoffset = 0;
    uint32_t pa_cl_vport_zscale = 0;
    uint32_t pa_cl_vte_cntl = 0;
    uint32_t pa_sc_screen_scissor_br = 0;
    uint32_t pa_sc_screen_scissor_tl = 0;
    uint32_t pa_sc_viz_query = 0;
    uint32_t pa_sc_window_offset = 0;
    uint32_t pa_sc_window_scissor_br = 0;
    uint32_t pa_sc_window_scissor_tl = 0;
    uint32_t pa_su_poly_offset_front_offset = 0;
    uint32_t pa_su_poly_offset_front_scale = 0;
    uint32_t pa_su_sc_mode_cntl = 0;
    uint32_t pa_su_vtx_cntl = 0;
    uint32_t rb_alpha_ref = 0;
    uint32_t rb_blendcontrol0 = 0;
    uint32_t rb_blendcontrol1 = 0;
    uint32_t rb_blendcontrol2 = 0;
    uint32_t rb_blendcontrol3 = 0;
    uint32_t rb_blend_alpha = 0;
    uint32_t rb_blend_blue = 0;
    uint32_t rb_blend_green = 0;
    uint32_t rb_blend_red = 0;
    uint32_t rb_color1_info = 0;
    uint32_t rb_color2_info = 0;
    uint32_t rb_color3_info = 0;
    uint32_t rb_colorcontrol = 0;
    uint32_t rb_color_info = 0;
    uint32_t rb_color_mask = 0;
    uint32_t rb_depthcontrol = 0;
    uint32_t rb_depth_info = 0;
    uint32_t rb_modecontrol = 0;
    uint32_t rb_stencilrefmask = 0;
    uint32_t rb_stencilrefmask_bf = 0;
    uint32_t rb_surface_info = 0;
    uint32_t vgt_indx_offset = 0;
    uint32_t vgt_multi_prim_ib_reset_indx = 0;
    std::array<uint32_t, 2048> floats{};
    std::array<uint32_t, 192> fetch{};
    std::array<uint32_t, 8> bools{};
    std::array<uint32_t, 32> loops{};
    struct Scissor { int32_t x0=0, y0=0, x1=0, y1=0; } scissor;
    std::array<TargetDesc, 4> colors{};
    TargetDesc depth;
    std::array<Handle, 32> textures{};
    std::shared_ptr<const game_shader::Resource> vertex, pixel;
    const uint32_t* Words(uint32_t index, uint32_t count = 1) const {
        using namespace xe::gpu;
        auto range = [=](uint32_t start, const auto& data) -> const uint32_t* {
            return index >= start && index-start <= data.size() && count <= data.size()-(index-start) ? data.data()+index-start : nullptr;
        };
        if(auto p=range(XE_GPU_REG_SHADER_CONSTANT_000_X,floats)) return p;
        if(auto p=range(XE_GPU_REG_SHADER_CONSTANT_FETCH_00_0,fetch)) return p;
        if(auto p=range(XE_GPU_REG_SHADER_CONSTANT_BOOL_000_031,bools)) return p;
        if(auto p=range(XE_GPU_REG_SHADER_CONSTANT_LOOP_00,loops)) return p;
        throw std::out_of_range("native shader state range");
    }
    uint32_t Word(uint32_t index) const {
        using namespace xe::gpu;
        switch(index) {
        case XE_GPU_REG_PA_CL_CLIP_CNTL: return pa_cl_clip_cntl;
        case XE_GPU_REG_PA_CL_VPORT_XOFFSET: return pa_cl_vport_xoffset;
        case XE_GPU_REG_PA_CL_VPORT_XSCALE: return pa_cl_vport_xscale;
        case XE_GPU_REG_PA_CL_VPORT_YOFFSET: return pa_cl_vport_yoffset;
        case XE_GPU_REG_PA_CL_VPORT_YSCALE: return pa_cl_vport_yscale;
        case XE_GPU_REG_PA_CL_VPORT_ZOFFSET: return pa_cl_vport_zoffset;
        case XE_GPU_REG_PA_CL_VPORT_ZSCALE: return pa_cl_vport_zscale;
        case XE_GPU_REG_PA_CL_VTE_CNTL: return pa_cl_vte_cntl;
        case XE_GPU_REG_PA_SC_SCREEN_SCISSOR_BR: return pa_sc_screen_scissor_br;
        case XE_GPU_REG_PA_SC_SCREEN_SCISSOR_TL: return pa_sc_screen_scissor_tl;
        case XE_GPU_REG_PA_SC_VIZ_QUERY: return pa_sc_viz_query;
        case XE_GPU_REG_PA_SC_WINDOW_OFFSET: return pa_sc_window_offset;
        case XE_GPU_REG_PA_SC_WINDOW_SCISSOR_BR: return pa_sc_window_scissor_br;
        case XE_GPU_REG_PA_SC_WINDOW_SCISSOR_TL: return pa_sc_window_scissor_tl;
        case XE_GPU_REG_PA_SU_POLY_OFFSET_FRONT_OFFSET: return pa_su_poly_offset_front_offset;
        case XE_GPU_REG_PA_SU_POLY_OFFSET_FRONT_SCALE: return pa_su_poly_offset_front_scale;
        case XE_GPU_REG_PA_SU_SC_MODE_CNTL: return pa_su_sc_mode_cntl;
        case XE_GPU_REG_PA_SU_VTX_CNTL: return pa_su_vtx_cntl;
        case XE_GPU_REG_RB_ALPHA_REF: return rb_alpha_ref;
        case XE_GPU_REG_RB_BLENDCONTROL0: return rb_blendcontrol0;
        case XE_GPU_REG_RB_BLENDCONTROL1: return rb_blendcontrol1;
        case XE_GPU_REG_RB_BLENDCONTROL2: return rb_blendcontrol2;
        case XE_GPU_REG_RB_BLENDCONTROL3: return rb_blendcontrol3;
        case XE_GPU_REG_RB_BLEND_ALPHA: return rb_blend_alpha;
        case XE_GPU_REG_RB_BLEND_BLUE: return rb_blend_blue;
        case XE_GPU_REG_RB_BLEND_GREEN: return rb_blend_green;
        case XE_GPU_REG_RB_BLEND_RED: return rb_blend_red;
        case XE_GPU_REG_RB_COLOR1_INFO: return rb_color1_info;
        case XE_GPU_REG_RB_COLOR2_INFO: return rb_color2_info;
        case XE_GPU_REG_RB_COLOR3_INFO: return rb_color3_info;
        case XE_GPU_REG_RB_COLORCONTROL: return rb_colorcontrol;
        case XE_GPU_REG_RB_COLOR_INFO: return rb_color_info;
        case XE_GPU_REG_RB_COLOR_MASK: return rb_color_mask;
        case XE_GPU_REG_RB_DEPTHCONTROL: return rb_depthcontrol;
        case XE_GPU_REG_RB_DEPTH_INFO: return rb_depth_info;
        case XE_GPU_REG_RB_MODECONTROL: return rb_modecontrol;
        case XE_GPU_REG_RB_STENCILREFMASK: return rb_stencilrefmask;
        case XE_GPU_REG_RB_STENCILREFMASK_BF: return rb_stencilrefmask_bf;
        case XE_GPU_REG_RB_SURFACE_INFO: return rb_surface_info;
        case XE_GPU_REG_VGT_INDX_OFFSET: return vgt_indx_offset;
        case XE_GPU_REG_VGT_MULTI_PRIM_IB_RESET_INDX: return vgt_multi_prim_ib_reset_indx;
        default: return *Words(index);
        }
    }
    // Verified USA FlushState mirrors. This reads the CPU-side D3D object;
    // neither a CP register array nor a decoded GPU packet is involved.
    static uint32_t MirrorOffset(uint32_t field) {
        constexpr uint32_t starts[]={0x2000,0x2100,0x2180,0x2200,0x2280,0x2300,0x2380};
        constexpr uint32_t words[]={19,21,5,12,21,38,8};
        constexpr uint32_t offsets[]={0x2CC0,0x2D0C,0x2D60,0x2D74,0x2DA4,0x2DF8,0x2E90};
        for(unsigned i=0;i<7;++i) if(field>=starts[i] && field-starts[i]<words[i]) return offsets[i]+(field-starts[i])*4;
        throw std::out_of_range("native D3D field has no verified mirror");
    }
    template<class Reader> static DrawState Capture(uint32_t device, const Reader& read) {
        if(!game_shader::GuestRange(device,0x2EB0)) throw std::out_of_range("native D3D device address");
        using namespace xe::gpu;
        DrawState out;
        out.pa_cl_clip_cntl = read(device + MirrorOffset(XE_GPU_REG_PA_CL_CLIP_CNTL));
        out.pa_cl_vport_xoffset = read(device + MirrorOffset(XE_GPU_REG_PA_CL_VPORT_XOFFSET));
        out.pa_cl_vport_xscale = read(device + MirrorOffset(XE_GPU_REG_PA_CL_VPORT_XSCALE));
        out.pa_cl_vport_yoffset = read(device + MirrorOffset(XE_GPU_REG_PA_CL_VPORT_YOFFSET));
        out.pa_cl_vport_yscale = read(device + MirrorOffset(XE_GPU_REG_PA_CL_VPORT_YSCALE));
        out.pa_cl_vport_zoffset = read(device + MirrorOffset(XE_GPU_REG_PA_CL_VPORT_ZOFFSET));
        out.pa_cl_vport_zscale = read(device + MirrorOffset(XE_GPU_REG_PA_CL_VPORT_ZSCALE));
        out.pa_cl_vte_cntl = read(device + MirrorOffset(XE_GPU_REG_PA_CL_VTE_CNTL));
        out.pa_sc_screen_scissor_br = read(device + MirrorOffset(XE_GPU_REG_PA_SC_SCREEN_SCISSOR_BR));
        out.pa_sc_screen_scissor_tl = read(device + MirrorOffset(XE_GPU_REG_PA_SC_SCREEN_SCISSOR_TL));
        out.pa_sc_viz_query = read(device + MirrorOffset(XE_GPU_REG_PA_SC_VIZ_QUERY));
        out.pa_su_poly_offset_front_offset = read(device + MirrorOffset(XE_GPU_REG_PA_SU_POLY_OFFSET_FRONT_OFFSET));
        out.pa_su_poly_offset_front_scale = read(device + MirrorOffset(XE_GPU_REG_PA_SU_POLY_OFFSET_FRONT_SCALE));
        out.pa_su_sc_mode_cntl = read(device + MirrorOffset(XE_GPU_REG_PA_SU_SC_MODE_CNTL));
        out.pa_su_vtx_cntl = read(device + MirrorOffset(XE_GPU_REG_PA_SU_VTX_CNTL));
        out.rb_alpha_ref = read(device + MirrorOffset(XE_GPU_REG_RB_ALPHA_REF));
        out.rb_blendcontrol0 = read(device + MirrorOffset(XE_GPU_REG_RB_BLENDCONTROL0));
        out.rb_blendcontrol1 = read(device + MirrorOffset(XE_GPU_REG_RB_BLENDCONTROL1));
        out.rb_blendcontrol2 = read(device + MirrorOffset(XE_GPU_REG_RB_BLENDCONTROL2));
        out.rb_blendcontrol3 = read(device + MirrorOffset(XE_GPU_REG_RB_BLENDCONTROL3));
        out.rb_blend_alpha = read(device + MirrorOffset(XE_GPU_REG_RB_BLEND_ALPHA));
        out.rb_blend_blue = read(device + MirrorOffset(XE_GPU_REG_RB_BLEND_BLUE));
        out.rb_blend_green = read(device + MirrorOffset(XE_GPU_REG_RB_BLEND_GREEN));
        out.rb_blend_red = read(device + MirrorOffset(XE_GPU_REG_RB_BLEND_RED));
        out.rb_color1_info = read(device + MirrorOffset(XE_GPU_REG_RB_COLOR1_INFO));
        out.rb_color2_info = read(device + MirrorOffset(XE_GPU_REG_RB_COLOR2_INFO));
        out.rb_color3_info = read(device + MirrorOffset(XE_GPU_REG_RB_COLOR3_INFO));
        out.rb_colorcontrol = read(device + MirrorOffset(XE_GPU_REG_RB_COLORCONTROL));
        out.rb_color_info = read(device + MirrorOffset(XE_GPU_REG_RB_COLOR_INFO));
        out.rb_color_mask = read(device + MirrorOffset(XE_GPU_REG_RB_COLOR_MASK));
        out.rb_depthcontrol = read(device + MirrorOffset(XE_GPU_REG_RB_DEPTHCONTROL));
        out.rb_depth_info = read(device + MirrorOffset(XE_GPU_REG_RB_DEPTH_INFO));
        out.rb_modecontrol = read(device + MirrorOffset(XE_GPU_REG_RB_MODECONTROL));
        out.rb_stencilrefmask = read(device + MirrorOffset(XE_GPU_REG_RB_STENCILREFMASK));
        out.rb_stencilrefmask_bf = read(device + MirrorOffset(XE_GPU_REG_RB_STENCILREFMASK_BF));
        out.rb_surface_info = read(device + MirrorOffset(XE_GPU_REG_RB_SURFACE_INFO));
        out.vgt_indx_offset = read(device + MirrorOffset(XE_GPU_REG_VGT_INDX_OFFSET));
        out.vgt_multi_prim_ib_reset_indx = read(device + MirrorOffset(XE_GPU_REG_VGT_MULTI_PRIM_IB_RESET_INDX));
        auto copy=[&](auto& data,uint32_t offset) { for(uint32_t i=0;i<data.size();++i) data[i]=read(device+offset+i*4); };
        copy(out.floats,0x780);copy(out.fetch,0x480);copy(out.bools,0x2780);copy(out.loops,0x27A0);
        // Window/scissor are supplied by the native caller: the game emits
        // these separately for EDRAM tiling, not in the mirrored state blocks.
        return out;
    }
};
struct Draw {
    DrawState state;
    uint32_t primitive = 4, count = 0, indexBase = 0, endian = 0;
    bool indexed = false, indices32 = false;
};
}
