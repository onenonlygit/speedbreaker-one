// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <stdexcept>
#include <cstdint>
#include <cmath>

namespace gpu::native_render {
// Images are single-sample, single-layer, mip zero and already GENERAL. The
// owner supplies write/read dependencies and retains allocation until retirement.
struct ImageRegion {
    VkImage image = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t width = 0, height = 0;
    bool depth = false;
};
inline void CheckRect(const ImageRegion& image, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!image.image || !image.width || !image.height || x > image.width || y > image.height ||
        width > image.width-x || height > image.height-y)
        throw std::out_of_range("native image rectangle");
}
inline void RecordCopy(VkCommandBuffer cmd, const ImageRegion& source, const ImageRegion& dest,
    uint32_t sourceX, uint32_t sourceY, uint32_t destX, uint32_t destY, uint32_t width, uint32_t height) {
    if (!cmd || source.image==dest.image || source.depth!=dest.depth || source.format!=dest.format)
        throw std::invalid_argument("native copy requires distinct compatible images");
    CheckRect(source,sourceX,sourceY,width,height);CheckRect(dest,destX,destY,width,height);
    if (!width || !height) return;
    std::array<VkImageCopy,2> regions{};
    const unsigned count=source.depth ? 2 : 1;
    for (unsigned i=0;i<count;++i) {
        auto& r=regions[i];
        auto aspect=source.depth ? (i ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT) : VK_IMAGE_ASPECT_COLOR_BIT;
        r.srcSubresource={VkImageAspectFlags(aspect),0,0,1};r.dstSubresource=r.srcSubresource;
        r.srcOffset={int32_t(sourceX),int32_t(sourceY),0};r.dstOffset={int32_t(destX),int32_t(destY),0};r.extent={width,height,1};
    }
    vkCmdCopyImage(cmd,source.image,VK_IMAGE_LAYOUT_GENERAL,dest.image,VK_IMAGE_LAYOUT_GENERAL,count,regions.data());
}
inline uint32_t ReadbackBytesPerPixel(const ImageRegion& image, bool stencil) {
    if (stencil) {
        if (!image.depth) throw std::invalid_argument("stencil readback requires depth/stencil image");
        return 1;
    }
    if (image.depth) return 4; // the native depth attachment is D32_SFLOAT_S8_UINT
    switch (image.format) {
    case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
    case VK_FORMAT_R16G16_SNORM: case VK_FORMAT_R16G16_SFLOAT: case VK_FORMAT_R32_SFLOAT: return 4;
    case VK_FORMAT_R16G16B16A16_SNORM: case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R32G32_SFLOAT: return 8;
    default: throw std::invalid_argument("unsupported native readback format");
    }
}
inline void RecordReadback(VkCommandBuffer cmd, const ImageRegion& image, VkBuffer buffer, uint64_t bufferBytes,
    uint32_t x, uint32_t y, uint32_t width, uint32_t height, bool stencil=false) {
    CheckRect(image,x,y,width,height);
    const uint32_t bytesPerPixel=ReadbackBytesPerPixel(image,stencil);
    if (!cmd || !buffer || uint64_t(width)*height>bufferBytes/bytesPerPixel)
        throw std::invalid_argument("native readback buffer too small");
    if (!width || !height) return;
    VkBufferImageCopy copy{};
    copy.imageSubresource={image.depth ? (stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT) : VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    copy.imageOffset={int32_t(x),int32_t(y),0};copy.imageExtent={width,height,1};
    vkCmdCopyImageToBuffer(cmd,image.image,VK_IMAGE_LAYOUT_GENERAL,buffer,1,&copy);
}
// Call inside a compatible dynamic-rendering pass. Clear values are semantic
// floats/depth/stencil, with no Xbox packed depth/color clear register involved.
inline void RecordClear(VkCommandBuffer cmd, const ImageRegion& image, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height, const float* color, float depth, uint32_t stencil,
    bool clearDepth=true, bool clearStencil=true) {
    CheckRect(image,x,y,width,height);
    if (!cmd || (!image.depth && !color) || (image.depth && clearDepth && (!std::isfinite(depth) || depth<0 || depth>1))) throw std::invalid_argument("native clear arguments");
    if (!width || !height || (image.depth && !clearDepth && !clearStencil)) return;
    VkClearAttachment attachment{};
    if (image.depth) {
        attachment.aspectMask=(clearDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0)|(clearStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
        attachment.clearValue.depthStencil={depth,stencil};
    } else {
        attachment.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        for (unsigned i=0;i<4;++i) attachment.clearValue.color.float32[i]=color[i];
    }
    VkClearRect rect{{{int32_t(x),int32_t(y)},{width,height}},0,1};
    vkCmdClearAttachments(cmd,1,&attachment,1,&rect);
}
}
