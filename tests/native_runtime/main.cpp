// Production native renderer and owner thread; no PM4 decoder/register file.
#include <stdafx.h>
#include <gpu/native_worker.h>
#include <gpu/native_device.h>
#include <gpu/plume_backend.h>
#include <video/presenter.h>
#include <user/settings.h>
#include <user/portable.h>
#include <unistd.h>
#include <iostream>
namespace video {extern VulkanContext testContext;extern VkImage presented;}
namespace nr=gpu::native_render;
namespace worker=nr::worker;
static std::atomic<unsigned> errors{};
static VKAPI_ATTR VkBool32 VKAPI_CALL Debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,const VkDebugUtilsMessengerCallbackDataEXT* data,void*) {
    if(severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {++errors;std::cerr<<data->pMessage<<'\n';}return VK_FALSE;
}
static void Check(VkResult result) {assert(result==VK_SUCCESS);}
static auto Resource(uint64_t id,gpu::ShaderStage stage,const char* glsl) {
    auto r=std::make_shared<gpu::game_shader::Resource>();r->id=id;r->stage=stage;
    r->guestWords={uint32_t(id)};r->translated.ok=true;r->translated.glsl=glsl;
    if(stage==gpu::ShaderStage::Pixel) r->translated.colorTargetMask=1;return r;
}
static void Verify(const std::vector<uint8_t>& pixels) {
    assert(pixels.size()==1024);
    for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x) {
        bool inside=x>=2&&x<10&&y>=3&&y<12;
        const uint8_t expected[]={uint8_t(inside?64:0),uint8_t(inside?128:0),uint8_t(inside?191:0),255};
        assert(!memcmp(pixels.data()+(y*16+x)*4,expected,4));
    }
}
static std::vector<uint8_t> PresentedPixels() {
    auto& c=video::testContext;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=1024;bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer buffer;Check(vkCreateBuffer(c.device,&bi,nullptr,&buffer));
    VkMemoryRequirements requirements;vkGetBufferMemoryRequirements(c.device,buffer,&requirements);
    VkPhysicalDeviceMemoryProperties properties;vkGetPhysicalDeviceMemoryProperties(c.physical,&properties);
    uint32_t type=0;while(type<properties.memoryTypeCount&&(!(requirements.memoryTypeBits&(1u<<type))||
        !(properties.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))) ++type;
    assert(type<properties.memoryTypeCount);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=requirements.size;alloc.memoryTypeIndex=type;
    VkDeviceMemory memory;Check(vkAllocateMemory(c.device,&alloc,nullptr,&memory));Check(vkBindBufferMemory(c.device,buffer,memory,0));
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=c.queueFamily;
    VkCommandPool pool;Check(vkCreateCommandPool(c.device,&pi,nullptr,&pool));
    VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.commandBufferCount=1;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBuffer cmd;Check(vkAllocateCommandBuffers(c.device,&ca,&cmd));
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};Check(vkBeginCommandBuffer(cmd,&begin));
    VkMemoryBarrier ready{VK_STRUCTURE_TYPE_MEMORY_BARRIER};ready.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;ready.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&ready,0,nullptr,0,nullptr);
    VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={16,16,1};
    vkCmdCopyImageToBuffer(cmd,video::presented,VK_IMAGE_LAYOUT_GENERAL,buffer,1,&region);
    VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
    Check(vkEndCommandBuffer(cmd));VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};VkFence fence;Check(vkCreateFence(c.device,&fi,nullptr,&fence));
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&cmd;
    {std::lock_guard lock(*c.queueMutex);Check(vkQueueSubmit(c.queue,1,&submit,fence));}
    Check(vkWaitForFences(c.device,1,&fence,VK_TRUE,UINT64_MAX));
    void* mapped;Check(vkMapMemory(c.device,memory,0,VK_WHOLE_SIZE,0,&mapped));
    VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=memory;range.size=VK_WHOLE_SIZE;
    Check(vkInvalidateMappedMemoryRanges(c.device,1,&range));
    std::vector<uint8_t> bytes(static_cast<uint8_t*>(mapped),static_cast<uint8_t*>(mapped)+1024);
    vkUnmapMemory(c.device,memory);vkDestroyFence(c.device,fence,nullptr);vkDestroyCommandPool(c.device,pool,nullptr);
    vkDestroyBuffer(c.device,buffer,nullptr);vkFreeMemory(c.device,memory,nullptr);return bytes;
}
int main() {
    Check(volkInitialize());VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_2;
    const char* layer="VK_LAYER_KHRONOS_validation";const char* extension=VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    VkValidationFeatureEnableEXT sync=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validation{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    validation.enabledValidationFeatureCount=1;validation.pEnabledValidationFeatures=&sync;
    VkInstanceCreateInfo ic{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ic.pNext=&validation;ic.pApplicationInfo=&app;
    ic.enabledLayerCount=1;ic.ppEnabledLayerNames=&layer;ic.enabledExtensionCount=1;ic.ppEnabledExtensionNames=&extension;
    auto& ctx=video::testContext;Check(vkCreateInstance(&ic,nullptr,&ctx.instance));volkLoadInstance(ctx.instance);
    VkDebugUtilsMessengerCreateInfoEXT dc{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    dc.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dc.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dc.pfnUserCallback=Debug;VkDebugUtilsMessengerEXT messenger;Check(vkCreateDebugUtilsMessengerEXT(ctx.instance,&dc,nullptr,&messenger));
    uint32_t count=0;Check(vkEnumeratePhysicalDevices(ctx.instance,&count,nullptr));assert(count);
    std::vector<VkPhysicalDevice> physical(count);Check(vkEnumeratePhysicalDevices(ctx.instance,&count,physical.data()));ctx.physical=physical.front();
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical,&count,nullptr);std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical,&count,families.data());
    for(ctx.queueFamily=0;ctx.queueFamily<count;++ctx.queueFamily) if((families[ctx.queueFamily].queueFlags&3)==3) break;
    assert(ctx.queueFamily<count);float priority=1;
    VkDeviceQueueCreateInfo qc{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qc.queueFamilyIndex=ctx.queueFamily;qc.queueCount=1;qc.pQueuePriorities=&priority;
    VkPhysicalDeviceFeatures features{};vkGetPhysicalDeviceFeatures(ctx.physical,&features);
    const char* extensions[]={VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME};
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR};dynamic.dynamicRendering=VK_TRUE;
    VkDeviceCreateInfo dev{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dev.pNext=&dynamic;dev.queueCreateInfoCount=1;dev.pQueueCreateInfos=&qc;
    dev.enabledExtensionCount=2;dev.ppEnabledExtensionNames=extensions;dev.pEnabledFeatures=&features;
    Check(vkCreateDevice(ctx.physical,&dev,nullptr,&ctx.device));volkLoadDevice(ctx.device);vkGetDeviceQueue(ctx.device,ctx.queueFamily,0,&ctx.queue);
    static std::mutex queueMutex;ctx.queueMutex=&queueMutex;ctx.cmdBeginRendering=vkCmdBeginRenderingKHR;ctx.cmdEndRendering=vkCmdEndRenderingKHR;
    auto scratch=std::filesystem::temp_directory_path()/std::format("sb-native-{}",getpid());std::filesystem::create_directories(scratch);
    portable::CurrentLayout()={scratch,scratch,scratch};
    settings::detail::g_effective[size_t(settings::Id::InternalResolution)]=1;
    settings::detail::g_effective[size_t(settings::Id::GameShaderResources)]=1;
    settings::detail::g_effective[size_t(settings::Id::NativeVertexInput)]=1;
    assert(worker::Start());assert(worker::Running());
    bool rejected=false;try {gpu::renderer::Draw(0,0,0);} catch(const std::exception&) {rejected=true;}assert(rejected);
    nr::TargetDesc a{1,16,16,0,false},b{2,16,16,0,false},c{3,18,20,0,false};
    const float black[]={0,0,0,1};worker::Clear(a,0,0,16,16,black,1,0);
    nr::Draw draw;draw.count=3;auto& s=draw.state;s.colors[0]=a;
    s.vertex=Resource(1,gpu::ShaderStage::Vertex,R"(#version 450
void main(){vec2 p=vec2((gl_VertexIndex<<1)&2,gl_VertexIndex&2);gl_Position=vec4(p*2-1,.25,1);})");
    s.pixel=Resource(2,gpu::ShaderStage::Pixel,R"(#version 450
layout(location=0) out vec4 color;void main(){color=vec4(.25,.5,.75,1);})");
    s.rb_modecontrol=4;s.rb_color_mask=15;s.rb_blendcontrol0=0x00010001;
    s.pa_cl_vte_cntl=0x3f;s.pa_cl_vport_xscale=s.pa_cl_vport_yscale=std::bit_cast<uint32_t>(8.f);
    s.pa_cl_vport_xoffset=s.pa_cl_vport_yoffset=std::bit_cast<uint32_t>(8.f);
    s.pa_cl_vport_zscale=std::bit_cast<uint32_t>(1.f);s.scissor={2,3,10,12};
    worker::Draw(draw);auto pixels=worker::Readback(a,0,0,16,16);Verify(pixels);
    worker::Clear(b,0,0,16,16,black,1,0);s.colors[0]=b;s.textures[0]=a.handle;
    auto sample=Resource(3,gpu::ShaderStage::Pixel,R"(#version 450
layout(set=1,binding=0) uniform sampler2D source;
layout(location=0) out vec4 color;void main(){color=texture(source,gl_FragCoord.xy/vec2(16));})");
    sample->translated.texture2DMask=1;s.pixel=sample;s.fetch[3]=0x688u<<1;
    worker::Draw(draw);assert(worker::Readback(b,0,0,16,16)==pixels);
    // A 16-bit truncation of this count leaves two vertices and no triangle.
    s.pixel=Resource(4,gpu::ShaderStage::Pixel,R"(#version 450
layout(location=0) out vec4 color;void main(){color=vec4(.25,.5,.75,1);})");
    s.textures[0]=0;draw.count=65538;worker::Clear(b,0,0,16,16,black,1,0);worker::Draw(draw);
    assert(worker::Readback(b,0,0,16,16)==pixels);
    // Real translated vertex microcode and guest-endian hardware attributes.
    const uint32_t code[]={1u|(2u<<12)|(1u<<16),2u<<12,0,1u<<12,0x688u|(38u<<16),4u,
        62u|(1u<<15)|(15u<<16)|(50u<<26),0,(2u<<24)|(1u<<31)|(1u<<30)|(1u<<16)|(1u<<8)};
    std::vector<uint32_t> words;for(auto word:code) words.push_back(std::byteswap(word));
    auto container=std::make_shared<gpu::game_shader::Container>();container->stage=gpu::ShaderStage::Vertex;
    container->physicalBytes=uint32_t(words.size()*4);gpu::game_shader::Library library;
    auto translated=library.Create(0x1000,gpu::ShaderStage::Vertex,container,words);
    assert(translated&&translated->translated.ok&&!translated->translated.nativeAttributes.empty());
    assert(translated->id==1);s.vertex=translated;
    const float positions[]={-1,-1,.25,1,3,-1,.25,1,-1,3,.25,1};
    for(size_t i=0;i<std::size(positions);++i) {
        uint32_t word=std::byteswap(std::bit_cast<uint32_t>(positions[i]));memcpy(g_memory.base+0xA0010000ull+i*4,&word,4);
    }
    auto vf=translated->translated.nativeAttributes.front().constant*2;
    s.fetch[vf]=0x10003;s.fetch[vf+1]=(12u<<2)|2;draw.count=3;
    worker::Clear(b,0,0,16,16,black,1,0);worker::Draw(draw);assert(worker::Readback(b,0,0,16,16)==pixels);
    nr::TargetDesc depth{4,16,16,0,true};worker::Clear(depth,0,0,16,16,nullptr,1,19);
    s.colors={};s.depth=depth;s.rb_modecontrol=5;
    xe::gpu::reg::RB_DEPTHCONTROL control{};control.z_enable=1;control.z_write_enable=1;
    control.zfunc=xe::gpu::xenos::CompareFunction::kAlways;s.rb_depthcontrol=control.value;
    worker::Draw(draw);
    auto depths=worker::Readback(depth,0,0,16,16);auto stencil=worker::Readback(depth,0,0,16,16,true);
    assert(depths.size()==1024&&stencil.size()==256);
    for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x) {
        float z;memcpy(&z,depths.data()+(y*16+x)*4,4);
        assert(z==((x>=2&&x<10&&y>=3&&y<12)? .25f:1.f));assert(stencil[y*16+x]==19);
    }
    rejected=false;try {worker::Copy(a,a,0,0,0,0,1,1);} catch(const std::invalid_argument&) {rejected=true;}
    assert(rejected&&worker::Running());
    auto invalid=draw;invalid.state.colors[0]=b;invalid.state.depth={};invalid.state.rb_modecontrol=4;
    invalid.state.pixel=Resource(5,gpu::ShaderStage::Pixel,"#version 450\ninvalid shader source");
    rejected=false;try {worker::Draw(invalid);} catch(const std::runtime_error&) {rejected=true;}
    assert(rejected&&worker::Running());
    // An explicit native fence publishes completion without any PM4 event.
    auto fence=worker::Fence();assert(fence.wait_for(std::chrono::seconds(10))==std::future_status::ready);fence.get();
    std::array<std::thread,8> callers;
    for(auto& caller:callers) caller=std::thread([&]{assert(worker::Start());auto f=worker::Fence();f.get();});
    for(auto& caller:callers) caller.join();
    worker::Clear(c,0,0,18,20,black,1,0);worker::Copy(a,c,0,0,1,2,16,16);
    assert(worker::Readback(c,1,2,16,16)==pixels);
    assert(worker::Present(a,16,16));worker::Flush();assert(video::presented);assert(worker::Frames()==1);
    assert(PresentedPixels()==pixels);
    auto front=video::presented;worker::Destroy(a.handle);assert(video::presented==front);assert(PresentedPixels()==pixels);
    worker::Destroy(b.handle);worker::Destroy(c.handle);worker::Destroy(depth.handle);assert(worker::GetMemoryStats().targetCount==0);
    auto counters=gpu::plume_rhi::GetCounters();assert(counters.draws==5);assert(counters.dispatches==0);assert(errors==0);
    // Semantic CPU device bridge -> verified CPU mirrors -> owner -> real GPU.
    nr::Device device;
    auto surface=device.CreateSurface(0x5000,16,16,0,false);device.SetColor(0,0x5000);
    device.SetScissor({2,3,10,12});
    device.SetShaders(Resource(7,gpu::ShaderStage::Vertex,R"(#version 450
void main(){vec2 p=vec2((gl_VertexIndex<<1)&2,gl_VertexIndex&2);gl_Position=vec4(p*2-1,.25,1);})"),
        Resource(8,gpu::ShaderStage::Pixel,R"(#version 450
layout(location=0) out vec4 color;void main(){color=vec4(.25,.5,.75,1);})"));
    std::array<uint32_t,0x3400/4> mirrors{};
    auto field=[&](uint32_t reg,uint32_t value){mirrors[nr::DrawState::MirrorOffset(reg)/4]=value;};
    using namespace xe::gpu;
    field(XE_GPU_REG_RB_MODECONTROL,4);field(XE_GPU_REG_RB_COLOR_MASK,15);field(XE_GPU_REG_RB_BLENDCONTROL0,0x10001);
    field(XE_GPU_REG_PA_CL_VTE_CNTL,0x3f);field(XE_GPU_REG_PA_CL_VPORT_XSCALE,std::bit_cast<uint32_t>(8.f));
    field(XE_GPU_REG_PA_CL_VPORT_YSCALE,std::bit_cast<uint32_t>(8.f));field(XE_GPU_REG_PA_CL_VPORT_XOFFSET,std::bit_cast<uint32_t>(8.f));
    field(XE_GPU_REG_PA_CL_VPORT_YOFFSET,std::bit_cast<uint32_t>(8.f));field(XE_GPU_REG_PA_CL_VPORT_ZSCALE,std::bit_cast<uint32_t>(1.f));
    auto read=[&](uint32_t address){assert(address>=0x1000&&(address-0x1000)/4<mirrors.size());return mirrors[(address-0x1000)/4];};
    device.Clear(0x5000,0,0,16,16,black,1,0);device.DrawVertices(0x1000,read,4,0,3);
    assert(device.Readback(0x5000,0,0,16,16)==pixels);
    const uint16_t indices[]={std::byteswap(uint16_t(999)),0,std::byteswap(uint16_t(1)),std::byteswap(uint16_t(2))};
    memcpy(g_memory.base+0xA0020000ull,indices,sizeof(indices));
    device.Clear(0x5000,0,0,16,16,black,1,0);
    device.DrawIndexed(0x1000,read,4,{0x20000,sizeof(indices),false,1},1,3);
    assert(device.Readback(0x5000,0,0,16,16)==pixels);
    rejected=false;try {device.DrawIndexed(0x1000,read,4,{0x20000,sizeof(indices),false,1},2,3);} catch(const std::out_of_range&) {rejected=true;}
    assert(rejected);
    rejected=false;try {device.DrawIndexed(0x1000,read,4,{0x20000,sizeof(indices),true,2},UINT32_MAX,UINT32_MAX);} catch(const std::out_of_range&) {rejected=true;}
    assert(rejected);
    auto replacement=device.CreateSurface(0x5000,16,16,0,false);assert(replacement.handle!=surface.handle);
    device.Clear(0x5000,0,0,16,16,black,1,0);auto empty=device.Readback(0x5000,0,0,16,16);
    for(unsigned i=0;i<256;++i) {assert(empty[i*4]==0&&empty[i*4+1]==0&&empty[i*4+2]==0&&empty[i*4+3]==255);}
    device.DestroySurface(0x5000);assert(worker::GetMemoryStats().targetCount==0);
    auto bridged=gpu::plume_rhi::GetCounters();assert(bridged.draws==counters.draws+1&&bridged.indexedDraws==counters.indexedDraws+1);
    assert(bridged.dispatches==0&&errors==0);
    std::cout<<"Native owner: exact draw/sample/depth/copy output, presentation and retirement; zero validation errors\n"<<std::flush;
    // Production owner/completion/compiler threads live until process exit.
    std::_Exit(0);
}
