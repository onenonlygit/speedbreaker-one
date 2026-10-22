// SpeedBreaker One. GPL-3.0-or-later.
#include "plume_backend.h"
#include <cassert>
#include <atomic>
#include <cstdio>
#ifdef SB_PLUME_RHI
#include <plume_vulkan.h>
#include <memory>
#include <unordered_map>
#endif

namespace gpu::plume_rhi {
namespace {
bool enabled = false;
std::atomic<uint64_t> passes{}, draws{}, indexed{}, dispatches{}, graphics{}, compute{}, descriptors{}, images{};
#ifdef SB_PLUME_RHI
std::shared_ptr<plume::VulkanDevice> device;
struct Commands {
    plume::VulkanCommandList list;
    plume::VulkanPipelineLayout graphicsLayout{VkPipelineLayout(VK_NULL_HANDLE)}, computeLayout{VkPipelineLayout(VK_NULL_HANDLE)};
    explicit Commands(VkCommandBuffer cmd) : list(cmd, device.get()) {
        graphicsLayout.pushConstantRanges.resize(1);
        computeLayout.pushConstantRanges.resize(1);
    }
};
// The command processor owns recording. No worker mutates this command state.
thread_local std::unordered_map<VkCommandBuffer, std::unique_ptr<Commands>> commandLists;
thread_local Commands* active = nullptr;
thread_local VkCommandBuffer activeBuffer = VK_NULL_HANDLE;
Commands& Cmd(VkCommandBuffer cmd) { assert(active && activeBuffer == cmd); return *active; }
#endif
}
struct ImageAllocation {
#ifdef SB_PLUME_RHI
    // Keep the allocator alive until the final fence-retired image is freed.
    std::shared_ptr<plume::VulkanDevice> device;
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    ~ImageAllocation() { if (image) device->freeVulkanImage(image,allocation); }
#endif
};
VkResult AllocateImage(const VkImageCreateInfo& info, VkImage& image, VkDeviceSize& bytes, std::shared_ptr<ImageAllocation>& owner) {
#ifdef SB_PLUME_RHI
    if (enabled) {
        auto allocation = std::make_shared<ImageAllocation>(); allocation->device=device;
        VmaAllocationInfo memory{};
        auto result = device->allocateVulkanImage(info,&allocation->image,&allocation->allocation,&memory);
        if (result != VK_SUCCESS) return result;
        image=allocation->image; bytes=memory.size; owner=std::move(allocation); ++images;
        return VK_SUCCESS;
    }
#endif
    return VK_ERROR_FEATURE_NOT_PRESENT;
}
bool Initialize(VkInstance instance, VkPhysicalDevice physical, VkDevice nativeDevice, uint32_t family, bool requested) {
#ifdef SB_PLUME_RHI
    if (requested) {
        device = std::make_shared<plume::VulkanDevice>(instance, physical, nativeDevice, family);
        if (!device->isValid()) { device.reset(); fprintf(stderr,"[plume] shared-device allocator initialization failed\n"); return false; }
        enabled = true;
    }
#else
    if (requested) fprintf(stderr,"[plume] Plume is not compiled in; using Legacy Vulkan\n");
#endif
    fprintf(stderr,"[plume] backend=%s; instance/device/queue/loader owned by presenter\n", enabled ? "Plume Vulkan" : "Legacy Vulkan");
    return true;
}
bool Enabled() { return enabled; }
plume::RenderDevice* GetDevice() {
#ifdef SB_PLUME_RHI
    return enabled ? device.get() : nullptr;
#else
    return nullptr;
#endif
}
VkResult SubmitQueue(VkQueue queue, uint32_t count, const VkSubmitInfo* infos, VkFence fence) {
#ifdef SB_PLUME_RHI
    if (enabled) return device->submitVulkanQueue(queue,count,infos,fence);
#endif
    return vkQueueSubmit(queue,count,infos,fence);
}
void Shutdown() {
#ifdef SB_PLUME_RHI
    // Caller has retired all commands/resources, before destroying VkDevice.
    active = nullptr; activeBuffer = VK_NULL_HANDLE; commandLists.clear(); device.reset();
#endif
    enabled = false;
}
void BeginRecording(VkCommandBuffer cmd) {
#ifdef SB_PLUME_RHI
    if (enabled) {
        auto& slot=commandLists[cmd]; if (!slot) slot=std::make_unique<Commands>(cmd);
        active=slot.get(); activeBuffer=cmd; active->list.resetExternalState();
    }
#endif
}
void BeginRendering(VkCommandBuffer cmd, const VkRenderingInfoKHR* info, PFN_vkCmdBeginRenderingKHR legacy) {
#ifdef SB_PLUME_RHI
    if (enabled) { Cmd(cmd).list.beginVulkanRendering(*info); ++passes; return; }
#endif
    legacy(cmd,info);
}
void EndRendering(VkCommandBuffer cmd, PFN_vkCmdEndRenderingKHR legacy) {
#ifdef SB_PLUME_RHI
    if (enabled) { Cmd(cmd).list.endVulkanRendering(); return; }
#endif
    legacy(cmd);
}
void BindPipeline(VkCommandBuffer cmd, VkPipelineBindPoint point, VkPipeline pipeline) {
#ifdef SB_PLUME_RHI
    if (enabled) {
        if (point==VK_PIPELINE_BIND_POINT_GRAPHICS) { plume::VulkanGraphicsPipeline borrowed(pipeline); Cmd(cmd).list.setPipeline(&borrowed); }
        else { assert(point==VK_PIPELINE_BIND_POINT_COMPUTE); plume::VulkanComputePipeline borrowed(pipeline); Cmd(cmd).list.setPipeline(&borrowed); }
        return;
    }
#endif
    vkCmdBindPipeline(cmd,point,pipeline);
}
void PushConstants(VkCommandBuffer cmd, VkPipelineLayout layout, VkShaderStageFlags stages, uint32_t offset, uint32_t size, const void* data) {
#ifdef SB_PLUME_RHI
    if (enabled) {
        auto& c=Cmd(cmd);
        if (stages & VK_SHADER_STAGE_COMPUTE_BIT) {
            assert(stages==VK_SHADER_STAGE_COMPUTE_BIT);
            c.computeLayout.vk=layout; c.computeLayout.pushConstantRanges[0]={stages,0,offset+size};
            c.list.setComputePipelineLayout(&c.computeLayout); c.list.setComputePushConstants(0,data,offset,size);
        } else {
            c.graphicsLayout.vk=layout; c.graphicsLayout.pushConstantRanges[0]={stages,0,offset+size};
            c.list.setGraphicsPipelineLayout(&c.graphicsLayout); c.list.setGraphicsPushConstants(0,data,offset,size);
        }
        return;
    }
#endif
    vkCmdPushConstants(cmd,layout,stages,offset,size,data);
}
void BindDescriptorSets(VkCommandBuffer cmd, VkPipelineBindPoint point, VkPipelineLayout layout, uint32_t first, uint32_t count, const VkDescriptorSet* sets, uint32_t offsetCount, const uint32_t* offsets) {
#ifdef SB_PLUME_RHI
    if (enabled) { Cmd(cmd).list.bindVulkanDescriptorSets(point,layout,first,count,sets,offsetCount,offsets); ++descriptors; return; }
#endif
    vkCmdBindDescriptorSets(cmd,point,layout,first,count,sets,offsetCount,offsets);
}
void PushDescriptors(PFN_vkCmdPushDescriptorSetKHR push, VkCommandBuffer cmd, VkPipelineBindPoint point, VkPipelineLayout layout, uint32_t set, uint32_t count, const VkWriteDescriptorSet* writes) {
#ifdef SB_PLUME_RHI
    if (enabled) { Cmd(cmd).list.pushVulkanDescriptors(push,point,layout,set,count,writes); ++descriptors; return; }
#endif
    push(cmd,point,layout,set,count,writes);
}
void BindVertexBuffers(VkCommandBuffer cmd, uint32_t first, uint32_t count, const VkBuffer* buffers, const VkDeviceSize* offsets) {
#ifdef SB_PLUME_RHI
    if(enabled) {
        assert(count<=16);
        plume::VulkanBuffer wrapped[16];plume::RenderVertexBufferView views[16];plume::RenderInputSlot slots[16];
        for(uint32_t i=0;i<count;++i) { wrapped[i].vk=buffers[i];views[i].buffer={&wrapped[i],offsets[i]};slots[i].index=first+i; }
        Cmd(cmd).list.setVertexBuffers(first,views,count,slots);return;
    }
#endif
    vkCmdBindVertexBuffers(cmd,first,count,buffers,offsets);
}
void Draw(VkCommandBuffer cmd, uint32_t count, uint32_t instances, uint32_t first, uint32_t firstInstance) {
#ifdef SB_PLUME_RHI
    if (enabled) { Cmd(cmd).list.drawInstanced(count,instances,first,firstInstance); ++draws; return; }
#endif
    vkCmdDraw(cmd,count,instances,first,firstInstance);
}
void DrawIndexed(VkCommandBuffer cmd, uint32_t count, uint32_t instances, uint32_t first, int32_t base, uint32_t firstInstance) {
#ifdef SB_PLUME_RHI
    if (enabled) { Cmd(cmd).list.drawIndexedInstanced(count,instances,first,base,firstInstance); ++indexed; return; }
#endif
    vkCmdDrawIndexed(cmd,count,instances,first,base,firstInstance);
}
void Dispatch(VkCommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z) {
#ifdef SB_PLUME_RHI
    if (enabled) { Cmd(cmd).list.dispatch(x,y,z); ++dispatches; return; }
#endif
    vkCmdDispatch(cmd,x,y,z);
}
void SetViewport(VkCommandBuffer cmd, uint32_t first, uint32_t count, const VkViewport* viewports) {
#ifdef SB_PLUME_RHI
    if (enabled && first==0 && count==1) { const auto& v=*viewports; plume::RenderViewport p{v.x,v.y,v.width,v.height,v.minDepth,v.maxDepth}; Cmd(cmd).list.setViewports(&p,1); return; }
#endif
    vkCmdSetViewport(cmd,first,count,viewports);
}
void SetScissor(VkCommandBuffer cmd, uint32_t first, uint32_t count, const VkRect2D* scissors) {
#ifdef SB_PLUME_RHI
    if (enabled && first==0 && count==1) { const auto& s=*scissors; plume::RenderRect p{s.offset.x,s.offset.y,s.offset.x+int32_t(s.extent.width),s.offset.y+int32_t(s.extent.height)}; Cmd(cmd).list.setScissors(&p,1); return; }
#endif
    vkCmdSetScissor(cmd,first,count,scissors);
}
void SetDepthBias(VkCommandBuffer cmd, float constant, float clamp, float slope) {
#ifdef SB_PLUME_RHI
    if (enabled) { Cmd(cmd).list.setDepthBias(constant,clamp,slope); return; }
#endif
    vkCmdSetDepthBias(cmd,constant,clamp,slope);
}
VkResult CreateGraphicsPipelines(VkDevice nativeDevice, VkPipelineCache cache, uint32_t count, const VkGraphicsPipelineCreateInfo* infos, const VkAllocationCallbacks* callbacks, VkPipeline* pipelines) {
#ifdef SB_PLUME_RHI
    if (enabled) { assert(nativeDevice==device->vk); auto r=device->createVulkanGraphicsPipelines(cache,count,infos,callbacks,pipelines); if(r==VK_SUCCESS) graphics+=count; return r; }
#endif
    return vkCreateGraphicsPipelines(nativeDevice,cache,count,infos,callbacks,pipelines);
}
VkResult CreateComputePipelines(VkDevice nativeDevice, VkPipelineCache cache, uint32_t count, const VkComputePipelineCreateInfo* infos, const VkAllocationCallbacks* callbacks, VkPipeline* pipelines) {
#ifdef SB_PLUME_RHI
    if (enabled) { assert(nativeDevice==device->vk); auto r=device->createVulkanComputePipelines(cache,count,infos,callbacks,pipelines); if(r==VK_SUCCESS) compute+=count; return r; }
#endif
    return vkCreateComputePipelines(nativeDevice,cache,count,infos,callbacks,pipelines);
}
Counters GetCounters() { return {passes.load(),draws.load(),indexed.load(),dispatches.load(),graphics.load(),compute.load(),descriptors.load(),images.load()}; }
}
