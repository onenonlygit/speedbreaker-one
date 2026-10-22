// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <memory>
namespace plume { struct RenderDevice; }
namespace gpu::plume_rhi {
bool Initialize(VkInstance instance, VkPhysicalDevice physical, VkDevice device, uint32_t queueFamily, bool requested);
bool Enabled();
plume::RenderDevice* GetDevice();
VkResult SubmitQueue(VkQueue queue, uint32_t count, const VkSubmitInfo* infos, VkFence fence);
struct ImageAllocation;
VkResult AllocateImage(const VkImageCreateInfo& info, VkImage& image, VkDeviceSize& bytes, std::shared_ptr<ImageAllocation>& owner);
void Shutdown();
void BeginRecording(VkCommandBuffer cmd);
void BeginRendering(VkCommandBuffer cmd, const VkRenderingInfoKHR* info, PFN_vkCmdBeginRenderingKHR legacy);
void EndRendering(VkCommandBuffer cmd, PFN_vkCmdEndRenderingKHR legacy);
void BindPipeline(VkCommandBuffer cmd, VkPipelineBindPoint point, VkPipeline pipeline);
void PushConstants(VkCommandBuffer cmd, VkPipelineLayout layout, VkShaderStageFlags stages, uint32_t offset, uint32_t size, const void* data);
void BindDescriptorSets(VkCommandBuffer cmd, VkPipelineBindPoint point, VkPipelineLayout layout, uint32_t first, uint32_t count, const VkDescriptorSet* sets, uint32_t offsetCount, const uint32_t* offsets);
void PushDescriptors(PFN_vkCmdPushDescriptorSetKHR push, VkCommandBuffer cmd, VkPipelineBindPoint point, VkPipelineLayout layout, uint32_t set, uint32_t count, const VkWriteDescriptorSet* writes);
void BindVertexBuffers(VkCommandBuffer cmd, uint32_t first, uint32_t count, const VkBuffer* buffers, const VkDeviceSize* offsets);
void Draw(VkCommandBuffer cmd, uint32_t count, uint32_t instances, uint32_t first, uint32_t firstInstance);
void DrawIndexed(VkCommandBuffer cmd, uint32_t count, uint32_t instances, uint32_t first, int32_t base, uint32_t firstInstance);
void Dispatch(VkCommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z);
void SetViewport(VkCommandBuffer cmd, uint32_t first, uint32_t count, const VkViewport* viewports);
void SetScissor(VkCommandBuffer cmd, uint32_t first, uint32_t count, const VkRect2D* scissors);
void SetDepthBias(VkCommandBuffer cmd, float constant, float clamp, float slope);
VkResult CreateGraphicsPipelines(VkDevice device, VkPipelineCache cache, uint32_t count, const VkGraphicsPipelineCreateInfo* infos, const VkAllocationCallbacks* callbacks, VkPipeline* pipelines);
VkResult CreateComputePipelines(VkDevice device, VkPipelineCache cache, uint32_t count, const VkComputePipelineCreateInfo* infos, const VkAllocationCallbacks* callbacks, VkPipeline* pipelines);
struct Counters { uint64_t passes, draws, indexedDraws, dispatches, graphicsPipelines, computePipelines, descriptors, images; };
Counters GetCounters();
}
