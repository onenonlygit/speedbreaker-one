// SpeedBreaker One. GPL-3.0-or-later. Synthetic shaders only, no game assets.
#include <plume_vulkan.h>
#include <gpu/plume_backend.h>
#include <gpu/native_image_commands.h>
#include <array>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <atomic>
namespace rhi=gpu::plume_rhi;
using namespace plume;
static std::atomic<unsigned> errors{};
static VKAPI_ATTR VkBool32 VKAPI_CALL Debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT* d,void*) {
    if(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) { ++errors; std::cerr<<d->pMessage<<'\n'; }
    return VK_FALSE;
}
static void Check(VkResult r) { if(r!=VK_SUCCESS) { std::cerr<<"Vulkan error "<<r<<'\n'; std::abort(); } }
static std::vector<uint32_t> Read(const std::string& f) {
    std::ifstream in(f,std::ios::binary|std::ios::ate);assert(in); auto n=in.tellg(); assert(n>0 && n%4==0);
    std::vector<uint32_t> b(size_t(n)/4);in.seekg(0);in.read(reinterpret_cast<char*>(b.data()),n);assert(in);return b;
}
static void ImageBarrier(VkCommandBuffer cmd,VkImage image,VkImageLayout old,VkImageLayout next,VkAccessFlags src,VkAccessFlags dst,VkImageAspectFlags aspect=VK_IMAGE_ASPECT_COLOR_BIT) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.image=image;b.oldLayout=old;b.newLayout=next;
    b.srcAccessMask=src;b.dstAccessMask=dst;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    b.subresourceRange={aspect,0,1,0,1};
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&b);
}
static void HostBarrier(VkCommandBuffer cmd,VkAccessFlags access,VkPipelineStageFlags stage) {
    VkMemoryBarrier memory{VK_STRUCTURE_TYPE_MEMORY_BARRIER};memory.srcAccessMask=access;memory.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cmd,stage,VK_PIPELINE_STAGE_HOST_BIT,0,1,&memory,0,nullptr,0,nullptr);
}
int main(int argc,char** argv) {
    assert(argc==2 || argc==3); const bool requested=argc==2; const std::string folder=argv[1]; Check(volkInitialize());
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_2;
    const char* ext=VK_EXT_DEBUG_UTILS_EXTENSION_NAME; const char* layer="VK_LAYER_KHRONOS_validation";
    VkDebugUtilsMessengerCreateInfoEXT debug{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;debug.pfnUserCallback=Debug;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&app;ici.enabledExtensionCount=1;ici.ppEnabledExtensionNames=&ext;
    VkValidationFeatureEnableEXT sync=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validation{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};validation.enabledValidationFeatureCount=1;validation.pEnabledValidationFeatures=&sync;validation.pNext=&debug;
    ici.enabledLayerCount=1;ici.ppEnabledLayerNames=&layer;ici.pNext=&validation;
    VkInstance instance;Check(vkCreateInstance(&ici,nullptr,&instance));volkLoadInstance(instance);
    VkDebugUtilsMessengerEXT messenger;Check(vkCreateDebugUtilsMessengerEXT(instance,&debug,nullptr,&messenger));
    uint32_t count=0;Check(vkEnumeratePhysicalDevices(instance,&count,nullptr));assert(count);
    std::vector<VkPhysicalDevice> physicals(count);Check(vkEnumeratePhysicalDevices(instance,&count,physicals.data()));auto physical=physicals.front();
    vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,nullptr);std::vector<VkQueueFamilyProperties> families(count);vkGetPhysicalDeviceQueueFamilyProperties(physical,&count,families.data());
    uint32_t family=0;while(family<count && (families[family].queueFlags&3)!=3) ++family;assert(family<count);
    float priority=1;VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qci.queueFamilyIndex=family;qci.queueCount=1;qci.pQueuePriorities=&priority;
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR};dynamic.dynamicRendering=VK_TRUE;
    const char* deviceExt=VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dci.pNext=&dynamic;dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;dci.enabledExtensionCount=1;dci.ppEnabledExtensionNames=&deviceExt;
    VkDevice native;Check(vkCreateDevice(physical,&dci,nullptr,&native));volkLoadDevice(native);VkQueue queue;vkGetDeviceQueue(native,family,0,&queue);
    auto gipa=vkGetInstanceProcAddr;auto createDevice=vkCreateDevice;
    assert(rhi::Initialize(instance,physical,native,family,requested));assert(rhi::Enabled()==requested);
    std::unique_ptr<VulkanDevice> fixture;
    if (!requested) fixture=std::make_unique<VulkanDevice>(instance,physical,native,family);
    auto* device=requested ? static_cast<VulkanDevice*>(rhi::GetDevice()) : fixture.get();assert(device->vk==native && device->physicalDevice==physical);
    assert(vkGetInstanceProcAddr==gipa && vkCreateDevice==createDevice);
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};poolInfo.queueFamilyIndex=family;poolInfo.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool;Check(vkCreateCommandPool(native,&poolInfo,nullptr,&pool));VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};allocate.commandPool=pool;allocate.commandBufferCount=1;allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBuffer cmd;Check(vkAllocateCommandBuffers(native,&allocate,&cmd));
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence fence;Check(vkCreateFence(native,&fenceInfo,nullptr,&fence));
    auto submit=[&] {VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};info.commandBufferCount=1;info.pCommandBuffers=&cmd;Check(rhi::SubmitQueue(queue,1,&info,fence));Check(vkWaitForFences(native,1,&fence,VK_TRUE,UINT64_MAX));Check(vkResetFences(native,1,&fence));};
    {
        auto v=Read(folder+"/triangle.vert.spv"), f=Read(folder+"/triangle.frag.spv"), cs=Read(folder+"/words.comp.spv");
        auto vs=device->createShader(v.data(),v.size()*4,"main",RenderShaderFormat::SPIRV);
        auto ps=device->createShader(f.data(),f.size()*4,"main",RenderShaderFormat::SPIRV);
        RenderPushConstantRange range(0,0,0,16,RenderShaderStageFlag::PIXEL);
        auto layout=device->createPipelineLayout(RenderPipelineLayoutDesc(&range,1,nullptr,0));
        RenderGraphicsPipelineDesc graphics;graphics.pipelineLayout=layout.get();graphics.vertexShader=vs.get();graphics.pixelShader=ps.get();graphics.renderTargetCount=1;graphics.depthClipEnabled=true;
        graphics.renderTargetFormat[0]=RenderFormat::R8G8B8A8_UNORM;graphics.renderTargetBlend[0]=RenderBlendDesc::Copy();
        auto pipeline=device->createGraphicsPipeline(graphics);assert(static_cast<VulkanGraphicsPipeline*>(pipeline.get())->vk);
        auto color=device->createTexture(RenderTextureDesc::ColorTarget(16,16,RenderFormat::R8G8B8A8_UNORM));
        const RenderTexture* attachment=color.get();auto framebuffer=device->createFramebuffer(RenderFramebufferDesc(&attachment,1));
        std::array<std::unique_ptr<RenderBuffer>,7> reads;
        for(auto& read:reads) read=device->createBuffer(RenderBufferDesc::ReadbackBuffer(1024));
        const float value[4]={0.25f,0.5f,0.75f,1};RenderViewport viewport(0,0,16,16);RenderRect scissor(2,3,10,12);
        auto image=static_cast<VulkanTexture*>(color.get())->vk;
        // First pilot: unmodified Plume graphics/pipeline/framebuffer/resource APIs.
        VulkanCommandList classic(cmd,device);classic.begin();
        RenderTextureBarrier write(color.get(),RenderTextureLayout::COLOR_WRITE);classic.barriers(RenderBarrierStage::GRAPHICS,nullptr,0,&write,1);
        classic.setFramebuffer(framebuffer.get());classic.clearColor(0,RenderColor(0,0,0,1),nullptr,0);
        classic.setPipeline(pipeline.get());classic.setGraphicsPipelineLayout(layout.get());classic.setGraphicsPushConstants(0,value);
        classic.setViewports(&viewport,1);classic.setScissors(&scissor,1);classic.drawInstanced(3,1,0,0);
        RenderTextureBarrier source(color.get(),RenderTextureLayout::COPY_SOURCE);classic.barriers(RenderBarrierStage::COPY,nullptr,0,&source,1);
        // Plume's current Vulkan copyTextureRegion doesn't implement image-to-buffer.
        // Keep diagnostic readback native, just as the game capture path does.
        VkBufferImageCopy classicCopy{};classicCopy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};classicCopy.imageExtent={16,16,1};
        vkCmdCopyImageToBuffer(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,static_cast<VulkanBuffer*>(reads[0].get())->vk,1,&classicCopy);
        HostBarrier(cmd,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);
        classic.end();submit();
        // Same shaders/state through a dynamic-rendering pipeline, preserving native create infos.
        VkPipelineShaderStageCreateInfo stages[2]{};for(int i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=i?static_cast<VulkanShader*>(ps.get())->vk:static_cast<VulkanShader*>(vs.get())->vk;stages[i].pName="main";}
        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;
        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};rs.lineWidth=1;rs.polygonMode=VK_POLYGON_MODE_FILL;
        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blend{};blend.colorWriteMask=15;
        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};cb.attachmentCount=1;cb.pAttachments=&blend;
        VkDynamicState states[]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};ds.dynamicStateCount=2;ds.pDynamicStates=states;
        VkFormat format=VK_FORMAT_R8G8B8A8_UNORM;VkPipelineRenderingCreateInfoKHR rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};rendering.colorAttachmentCount=1;rendering.pColorAttachmentFormats=&format;
        VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};gp.pNext=&rendering;gp.stageCount=2;gp.pStages=stages;gp.layout=static_cast<VulkanPipelineLayout*>(layout.get())->vk;gp.pVertexInputState=&vi;gp.pInputAssemblyState=&ia;gp.pViewportState=&vp;gp.pRasterizationState=&rs;gp.pMultisampleState=&ms;gp.pColorBlendState=&cb;gp.pDynamicState=&ds;
        VkPipeline dynamicPipeline;Check(rhi::CreateGraphicsPipelines(native,VK_NULL_HANDLE,1,&gp,nullptr,&dynamicPipeline));
        auto index=device->createBuffer(RenderBufferDesc::IndexBuffer(6,RenderHeapType::UPLOAD));uint16_t indices[]={0,1,2};std::memcpy(index->map(),indices,6);index->unmap();
        auto vertexBuffer=device->createBuffer(RenderBufferDesc::UploadBuffer(80,RenderBufferFlag::VERTEX));
        auto* host=static_cast<uint32_t*>(vertexBuffer->map());
        for(unsigned i=0;i<20;++i)host[i]=0;
        // Nonzero binding offset and vertex stride. Words are guest byte order.
        for(unsigned vertex=0;vertex<3;++vertex) { host[4+vertex*4]=0x0000803f;host[5+vertex*4]=0x00000040;host[6+vertex*4]=0x00008040;host[7+vertex*4]=0x00000041; }
        vertexBuffer->unmap();
        for(unsigned n=1;n<7;++n) {
            VkPipeline selected=dynamicPipeline;
            VkShaderModule nativeShader=VK_NULL_HANDLE;
            VkVertexInputBindingDescription binding{0,16,VK_VERTEX_INPUT_RATE_VERTEX};
            const VkFormat inputFormats[]={VK_FORMAT_R32_UINT,VK_FORMAT_R32G32_UINT,VK_FORMAT_R32G32B32_UINT,VK_FORMAT_R32G32B32A32_UINT};
            VkVertexInputAttributeDescription attribute{0,0,inputFormats[n>=3?n-3:0],0};
            if(n>=3) {
                auto binary=Read(folder+"/native"+std::to_string(n-2)+".vert.spv");
                VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=binary.size()*4;sm.pCode=binary.data();Check(vkCreateShaderModule(native,&sm,nullptr,&nativeShader));
                stages[0].module=nativeShader;vi.vertexBindingDescriptionCount=vi.vertexAttributeDescriptionCount=1;vi.pVertexBindingDescriptions=&binding;vi.pVertexAttributeDescriptions=&attribute;
                Check(vkCreateGraphicsPipelines(native,VK_NULL_HANDLE,1,&gp,nullptr,&selected));
            }
            Check(vkResetCommandBuffer(cmd,0));VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};Check(vkBeginCommandBuffer(cmd,&begin));rhi::BeginRecording(cmd);
            ImageBarrier(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            VkRenderingAttachmentInfoKHR colorInfo{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR};colorInfo.imageView=static_cast<VulkanTexture*>(color.get())->imageView;colorInfo.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;colorInfo.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;colorInfo.storeOp=VK_ATTACHMENT_STORE_OP_STORE;colorInfo.clearValue.color.float32[3]=1;
            VkRenderingInfoKHR ri{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};ri.renderArea.extent={16,16};ri.layerCount=1;ri.colorAttachmentCount=1;ri.pColorAttachments=&colorInfo;
            VkViewport nativeViewport{0,0,16,16,0,1};VkRect2D nativeScissor{{2,3},{8,9}};
            if(n==1) {
                vkCmdBeginRenderingKHR(cmd,&ri);vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,dynamicPipeline);vkCmdSetViewport(cmd,0,1,&nativeViewport);vkCmdSetScissor(cmd,0,1,&nativeScissor);vkCmdPushConstants(cmd,gp.layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,16,value);vkCmdDraw(cmd,3,1,0,0);vkCmdEndRenderingKHR(cmd);
            } else {
                rhi::BeginRendering(cmd,&ri,vkCmdBeginRenderingKHR);rhi::BindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,selected);
                if(n>=3) { VkBuffer vb=static_cast<VulkanBuffer*>(vertexBuffer.get())->vk;VkDeviceSize at=16;rhi::BindVertexBuffers(cmd,0,1,&vb,&at); }
                rhi::SetViewport(cmd,0,1,&nativeViewport);rhi::SetScissor(cmd,0,1,&nativeScissor);rhi::PushConstants(cmd,gp.layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,16,value);
                vkCmdBindIndexBuffer(cmd,static_cast<VulkanBuffer*>(index.get())->vk,0,VK_INDEX_TYPE_UINT16);rhi::DrawIndexed(cmd,3,1,0,0,0);rhi::EndRendering(cmd,vkCmdEndRenderingKHR);
            }
            ImageBarrier(cmd,image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
            VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={16,16,1};vkCmdCopyImageToBuffer(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,static_cast<VulkanBuffer*>(reads[n].get())->vk,1,&copy);HostBarrier(cmd,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);Check(vkEndCommandBuffer(cmd));submit();
            if(nativeShader) {vkDestroyPipeline(native,selected,nullptr);vkDestroyShaderModule(native,nativeShader,nullptr);}
        }
        RenderRange readRange(0,1024);auto* expected=reads[0]->map(0,&readRange);assert(expected);
        for(unsigned n=1;n<7;++n){auto* actual=reads[n]->map(0,&readRange);assert(!std::memcmp(expected,actual,1024));reads[n]->unmap();}
        // Use the actual translated shader from a host-owned game resource,
        // not the handwritten native-input pilot. Its guest vertex bytes are
        // decoded by the translator while Plume records the indexed draw.
        {
            auto binary=Read(folder+"/game-resource.vert.spv");
            auto shader=device->createShader(binary.data(),binary.size()*4,"main",RenderShaderFormat::SPIRV);
            auto constants=device->createBuffer(RenderBufferDesc::UploadBuffer(16384,RenderBufferFlag::CONSTANT));
            auto* data=static_cast<uint32_t*>(constants->map());std::memset(data,0,16384);
            data[8192/4+1]=2; // fetch constant zero: guest 8-in-32 endian swap
            constants->unmap();
            auto geometry=device->createBuffer(RenderBufferDesc::UploadBuffer(64,RenderBufferFlag::VERTEX));
            float positions[12]={-1,-1,0.5f,1, 3,-1,0.5f,1, -1,3,0.5f,1};
            auto* vertices=static_cast<uint32_t*>(geometry->map());std::memset(vertices,0,64);
            for(unsigned i=0;i<12;++i){uint32_t bits;std::memcpy(&bits,&positions[i],4);vertices[4+i]=__builtin_bswap32(bits);}
            geometry->unmap();
            VkDescriptorSetLayoutBinding uniform{1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr};
            VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};setInfo.bindingCount=1;setInfo.pBindings=&uniform;
            VkDescriptorSetLayout setLayout;Check(vkCreateDescriptorSetLayout(native,&setInfo,nullptr,&setLayout));
            VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1};
            VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};poolInfo.maxSets=1;poolInfo.poolSizeCount=1;poolInfo.pPoolSizes=&poolSize;
            VkDescriptorPool descriptorPool;Check(vkCreateDescriptorPool(native,&poolInfo,nullptr,&descriptorPool));
            VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};allocation.descriptorPool=descriptorPool;allocation.descriptorSetCount=1;allocation.pSetLayouts=&setLayout;
            VkDescriptorSet descriptor;Check(vkAllocateDescriptorSets(native,&allocation,&descriptor));
            VkDescriptorBufferInfo buffer{static_cast<VulkanBuffer*>(constants.get())->vk,0,16384};
            VkWriteDescriptorSet update{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};update.dstSet=descriptor;update.dstBinding=1;update.descriptorCount=1;update.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;update.pBufferInfo=&buffer;
            vkUpdateDescriptorSets(native,1,&update,0,nullptr);
            VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,112};
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layoutInfo.setLayoutCount=1;layoutInfo.pSetLayouts=&setLayout;layoutInfo.pushConstantRangeCount=1;layoutInfo.pPushConstantRanges=&push;
            VkPipelineLayout gameLayout;Check(vkCreatePipelineLayout(native,&layoutInfo,nullptr,&gameLayout));
            VkVertexInputBindingDescription binding{0,16,VK_VERTEX_INPUT_RATE_VERTEX};
            VkVertexInputAttributeDescription attribute{0,0,VK_FORMAT_R32G32B32A32_UINT,0};
            vi.vertexBindingDescriptionCount=vi.vertexAttributeDescriptionCount=1;vi.pVertexBindingDescriptions=&binding;vi.pVertexAttributeDescriptions=&attribute;
            stages[0].module=static_cast<VulkanShader*>(shader.get())->vk;gp.layout=gameLayout;
            VkPipeline gamePipeline;Check(rhi::CreateGraphicsPipelines(native,VK_NULL_HANDLE,1,&gp,nullptr,&gamePipeline));
            auto output=device->createBuffer(RenderBufferDesc::ReadbackBuffer(1024));
            Check(vkResetCommandBuffer(cmd,0));VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};Check(vkBeginCommandBuffer(cmd,&begin));rhi::BeginRecording(cmd);
            ImageBarrier(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            VkRenderingAttachmentInfoKHR attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR};attachment.imageView=static_cast<VulkanTexture*>(color.get())->imageView;attachment.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachment.clearValue.color.float32[3]=1;
            VkRenderingInfoKHR info{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};info.renderArea.extent={16,16};info.layerCount=1;info.colorAttachmentCount=1;info.pColorAttachments=&attachment;
            VkViewport view{0,0,16,16,0,1};VkRect2D clip{{2,3},{8,9}};
            std::array<uint32_t,28> pc{};std::memcpy(pc.data(),value,16);pc[7]=2; // normal W; ordinary mode ignores index-address words
            rhi::BeginRendering(cmd,&info,vkCmdBeginRenderingKHR);rhi::BindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,gamePipeline);
            rhi::BindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,gameLayout,0,1,&descriptor,0,nullptr);
            VkBuffer vb=static_cast<VulkanBuffer*>(geometry.get())->vk;VkDeviceSize offset=16;rhi::BindVertexBuffers(cmd,0,1,&vb,&offset);
            rhi::SetViewport(cmd,0,1,&view);rhi::SetScissor(cmd,0,1,&clip);rhi::PushConstants(cmd,gameLayout,push.stageFlags,0,112,pc.data());
            vkCmdBindIndexBuffer(cmd,static_cast<VulkanBuffer*>(index.get())->vk,0,VK_INDEX_TYPE_UINT16);rhi::DrawIndexed(cmd,3,1,0,0,0);rhi::EndRendering(cmd,vkCmdEndRenderingKHR);
            ImageBarrier(cmd,image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
            VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={16,16,1};vkCmdCopyImageToBuffer(cmd,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,static_cast<VulkanBuffer*>(output.get())->vk,1,&copy);
            HostBarrier(cmd,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);Check(vkEndCommandBuffer(cmd));submit();
            const void* actual=output->map(0,&readRange);assert(!std::memcmp(expected,actual,1024));output->unmap();
            vkDestroyPipeline(native,gamePipeline,nullptr);vkDestroyPipelineLayout(native,gameLayout,nullptr);vkDestroyDescriptorPool(native,descriptorPool,nullptr);vkDestroyDescriptorSetLayout(native,setLayout,nullptr);
        }
        auto* pixel=static_cast<const uint8_t*>(expected);assert(pixel[(4*16+4)*4]==64 && pixel[0]==0 && pixel[3]==255);reads[0]->unmap();
        vkDestroyPipeline(native,dynamicPipeline,nullptr);
        // The exact native image command helpers used by renderer.cpp:
        // partial semantic clears, image-resident copies, independent depth
        // and stencil aspects, unequal target dimensions and bounds rejection.
        for (bool depth : {false,true}) {
            namespace nr=gpu::native_render;
            const auto format=depth ? RenderFormat::D32_FLOAT_S8_UINT : RenderFormat::R8G8B8A8_UNORM;
            auto src=depth ? device->createTexture(RenderTextureDesc::DepthTarget(16,16,format)) : device->createTexture(RenderTextureDesc::ColorTarget(16,16,format));
            auto dst=depth ? device->createTexture(RenderTextureDesc::DepthTarget(18,20,format)) : device->createTexture(RenderTextureDesc::ColorTarget(18,20,format));
            auto* a=static_cast<VulkanTexture*>(src.get());auto* b=static_cast<VulkanTexture*>(dst.get());
            const VkImageAspectFlags aspect=depth ? VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            const VkFormat vkFormat=depth ? VK_FORMAT_D32_SFLOAT_S8_UINT : VK_FORMAT_R8G8B8A8_UNORM;
            nr::ImageRegion source{a->vk,vkFormat,16,16,depth},dest{b->vk,vkFormat,18,20,depth};
            auto output=device->createBuffer(RenderBufferDesc::ReadbackBuffer(18*20*4));
            auto stencilOutput=device->createBuffer(RenderBufferDesc::ReadbackBuffer(18*20));
            Check(vkResetCommandBuffer(cmd,0));VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};Check(vkBeginCommandBuffer(cmd,&begin));rhi::BeginRecording(cmd);
            ImageBarrier(cmd,a->vk,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,aspect);
            ImageBarrier(cmd,b->vk,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_TRANSFER_WRITE_BIT,aspect);
            VkRenderingAttachmentInfoKHR attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR};attachment.imageView=a->imageView;attachment.imageLayout=VK_IMAGE_LAYOUT_GENERAL;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
            if(depth) attachment.clearValue.depthStencil={0.75f,17};else attachment.clearValue.color.float32[3]=1;
            VkRenderingInfoKHR rendering{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};rendering.renderArea.extent={16,16};rendering.layerCount=1;
            if(depth) rendering.pDepthAttachment=rendering.pStencilAttachment=&attachment;
            else {rendering.colorAttachmentCount=1;rendering.pColorAttachments=&attachment;}
            rhi::BeginRendering(cmd,&rendering,vkCmdBeginRenderingKHR);
            nr::RecordClear(cmd,source,2,3,8,9,value,0.25f,91);
            if(depth) {
                nr::RecordClear(cmd,source,4,5,2,2,nullptr,0.5f,0,true,false);
                nr::RecordClear(cmd,source,6,7,2,2,nullptr,0.0f,7,false,true);
            }
            bool rejected=false;try {nr::RecordClear(cmd,source,UINT32_MAX,0,2,2,value,0,0);}catch(const std::out_of_range&){rejected=true;}assert(rejected);
            rhi::EndRendering(cmd,vkCmdEndRenderingKHR);
            VkImageSubresourceRange range{aspect,0,1,0,1};
            if(depth) {VkClearDepthStencilValue clear{1,0};vkCmdClearDepthStencilImage(cmd,b->vk,VK_IMAGE_LAYOUT_GENERAL,&clear,1,&range);}
            else {VkClearColorValue clear{};clear.float32[3]=1;vkCmdClearColorImage(cmd,b->vk,VK_IMAGE_LAYOUT_GENERAL,&clear,1,&range);}
            ImageBarrier(cmd,a->vk,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,aspect);
            ImageBarrier(cmd,b->vk,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,aspect);
            nr::RecordCopy(cmd,source,dest,2,3,5,6,8,9);
            rejected=false;try {nr::RecordCopy(cmd,source,dest,0,0,17,0,2,2);}catch(const std::out_of_range&){rejected=true;}assert(rejected);
            rejected=false;try {nr::RecordCopy(cmd,source,source,0,0,0,0,2,2);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
            ImageBarrier(cmd,b->vk,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,aspect);
            nr::RecordReadback(cmd,dest,static_cast<VulkanBuffer*>(output.get())->vk,18*20*4,0,0,18,20);
            if(depth) nr::RecordReadback(cmd,dest,static_cast<VulkanBuffer*>(stencilOutput.get())->vk,18*20,0,0,18,20,true);
            rejected=false;try {nr::RecordReadback(cmd,dest,static_cast<VulkanBuffer*>(output.get())->vk,1,0,0,18,20);}catch(const std::invalid_argument&){rejected=true;}assert(rejected);
            HostBarrier(cmd,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);Check(vkEndCommandBuffer(cmd));submit();
            auto* data=static_cast<const uint8_t*>(output->map());auto* stencil=static_cast<const uint8_t*>(stencilOutput->map());
            for(unsigned y=0;y<20;++y)for(unsigned x=0;x<18;++x) {
                const bool inside=x>=5 && x<13 && y>=6 && y<15;const auto pixel=y*18+x;
                if(depth) {
                    const bool depthOnly=x>=7 && x<9 && y>=8 && y<10;
                    const bool stencilOnly=x>=9 && x<11 && y>=10 && y<12;
                    float actual;std::memcpy(&actual,data+pixel*4,4);
                    assert(actual==(inside ? (depthOnly ? 0.5f : 0.25f) : 1.0f));
                    assert(stencil[pixel]==(inside ? (stencilOnly ? 7 : 91) : 0));
                } else {
                    assert(data[pixel*4]==(inside ? 64 : 0));assert(data[pixel*4+1]==(inside ? 128 : 0));
                    assert(data[pixel*4+2]==(inside ? 191 : 0));assert(data[pixel*4+3]==255);
                }
            }
            output->unmap();stencilOutput->unmap();
        }
        // Native image ownership uses Plume/VMA, including mutable formats.
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};imageInfo.imageType=VK_IMAGE_TYPE_2D;imageInfo.extent={16,16,1};imageInfo.mipLevels=imageInfo.arrayLayers=1;imageInfo.format=VK_FORMAT_R8G8B8A8_UNORM;imageInfo.samples=VK_SAMPLE_COUNT_1_BIT;imageInfo.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_STORAGE_BIT;imageInfo.flags=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        std::shared_ptr<rhi::ImageAllocation> owner;VkImage nativeImage;VkDeviceSize bytes=0;if (requested) { Check(rhi::AllocateImage(imageInfo,nativeImage,bytes,owner));assert(nativeImage && bytes);owner.reset(); }
        // Plume-owned compute pipeline/descriptors/storage; raw and Plume dispatch must match.
        for (const auto& bytecode : {cs, Read(folder+"/route.comp.spv")}) {
        auto shader=device->createShader(bytecode.data(),bytecode.size()*4,"main",RenderShaderFormat::SPIRV);
        RenderDescriptorRange bufferRange(RenderDescriptorRangeType::READ_WRITE_STRUCTURED_BUFFER,0,1);RenderDescriptorSetDesc setDesc(&bufferRange,1);
        RenderPushConstantRange push(0,0,0,4,RenderShaderStageFlag::COMPUTE);auto computeLayout=device->createPipelineLayout(RenderPipelineLayoutDesc(&push,1,&setDesc,1));
        auto computePipeline=device->createComputePipeline(RenderComputePipelineDesc(computeLayout.get(),shader.get(),64,1,1));
        auto output=device->createBuffer(RenderBufferDesc::ReadbackBuffer(257*4,RenderBufferFlag::STORAGE));auto set=device->createDescriptorSet(setDesc);set->setBuffer(0,output.get());
        const uint32_t seed=0x1234abcd;
        for(bool bridge:{false,true}) {
            Check(vkResetCommandBuffer(cmd,0));VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};Check(vkBeginCommandBuffer(cmd,&begin));rhi::BeginRecording(cmd);
            auto cp=static_cast<VulkanComputePipeline*>(computePipeline.get())->vk;auto cl=static_cast<VulkanPipelineLayout*>(computeLayout.get())->vk;auto ds=static_cast<VulkanDescriptorSet*>(set.get())->vk;
            if(bridge) {rhi::BindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,cp);rhi::BindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,cl,0,1,&ds,0,nullptr);rhi::PushConstants(cmd,cl,VK_SHADER_STAGE_COMPUTE_BIT,0,4,&seed);rhi::Dispatch(cmd,5,1,1);}
            else {vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,cp);vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,cl,0,1,&ds,0,nullptr);vkCmdPushConstants(cmd,cl,VK_SHADER_STAGE_COMPUTE_BIT,0,4,&seed);vkCmdDispatch(cmd,5,1,1);}
            HostBarrier(cmd,VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            Check(vkEndCommandBuffer(cmd));submit();RenderRange rr(0,257*4);auto* words=static_cast<uint32_t*>(output->map(0,&rr));for(uint32_t i=0;i<257;++i)assert(words[i]==((i*1664525u+1013904223u)^seed));output->unmap();
        }
    }
    }
    const auto c=rhi::GetCounters();
    if (requested) assert(c.passes==8 && c.indexedDraws==6 && c.dispatches==2 && c.images==1);
    else assert(c.passes==0 && c.indexedDraws==0 && c.dispatches==0 && c.images==0);
    rhi::Shutdown(); fixture.reset(); // Must leave native device and dispatch alive.
    assert(vkGetInstanceProcAddr==gipa);Check(vkDeviceWaitIdle(native));
    vkDestroyFence(native,fence,nullptr);vkDestroyCommandPool(native,pool,nullptr);vkDestroyDevice(native,nullptr);vkDestroyDebugUtilsMessengerEXT(instance,messenger,nullptr);vkDestroyInstance(instance,nullptr);
    assert(errors==0);std::cout<<"Plume classic/offscreen and raw/dynamic parity, indexed draw, 257 compute words, native partial clears/copies/depth/stencil, ownership and shared dispatch passed; validation errors=0\n";
}
