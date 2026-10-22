// Immutable-memory test platform. Does not validate CPU locks/write protection.
#include <stdafx.h>
#include <kernel/vmem.h>
#include <kernel/write_watch.h>
#include <video/presenter.h>
#include <video/picture_fit.h>
#include <user/settings.h>
#include <cpu/guest_time.h>
#include <debug/timeline.h>
#include <report/zip.h>
Memory::Memory() {
    base=static_cast<uint8_t*>(mmap(nullptr,PPC_MEMORY_SIZE,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE,-1,0));assert(base!=MAP_FAILED);
}
Memory g_memory;
namespace vmem {
PageHeap::PageHeap(uint32_t base,uint32_t size,uint32_t pageSize):m_base(base),m_size(size),m_pageSize(pageSize) {}
PageHeap& Physical() {static PageHeap heap(0xA0000000,512*1024*1024,4096);return heap;}
}
namespace settings::detail {
std::atomic<uint8_t> g_state{1};std::atomic<double> g_effective[kCount]{};
std::atomic<uint64_t> g_generation{1};void InitBeforeLoad(Id) {std::abort();}
}
namespace timeline {bool g_enabled=false;void Record(Kind,uint32_t) {std::abort();}}
namespace hostcpu {void LeaveReservedCore() {} void UseReservedCore(const char*) {}}
void SetHostThreadName(const char*) {}
namespace guesttime {State g_state;bool Suspended() {return false;}void WaitWhileSuspendedSlow() {std::abort();}}
namespace report {std::vector<uint8_t> EncodePng(const uint8_t*,uint32_t,uint32_t) {std::abort();}}
namespace video {
VulkanContext testContext{};VkImage presented=VK_NULL_HANDLE;
const VulkanContext* GetVulkan() {return &testContext;}
void WaitGpuIdle() {assert(vkDeviceWaitIdle(testContext.device)==VK_SUCCESS);}
void WaitForeground(std::unique_lock<std::mutex>&) {}
void ReleaseFrontImage(VkImage image) {if(presented==image) presented=VK_NULL_HANDLE;}
void SubmitFrontImage(VkImage image,uint32_t,uint32_t,uint32_t,uint32_t) {presented=image;}
void FrameRegion(uint32_t& w,uint32_t& h) {w=h=16;}
void AutoRenderScale(uint32_t,uint32_t,uint32_t,uint32_t& x,uint32_t& y) {x=y=1;}
}
namespace game {
float AspectScaleX() {return 1;}float AspectScaleY() {return 1;}
float FrontEndScaleX() {return 1;}float FrontEndScaleY() {return 1;}
}
namespace writewatch {
std::atomic<uint32_t> g_snapsOpen{0};std::atomic<bool> g_submitRequested{false};
thread_local bool t_gpuThread=false;static uint8_t* shadow;
void EnableShadow(uint8_t* p) {shadow=p;}
uint64_t SyncShadow(uint32_t p,uint32_t n,uint64_t) {
    assert(uint64_t(p)+n<=512*1024*1024);memcpy(shadow+p,g_memory.base+0xA0000000ull+p,n);return 0;
}
uint64_t Current() {return 1;}uint64_t WriteEpoch() {return 1;}
uint64_t WriteSequence(uint32_t,uint32_t) {return 1;}
bool WrittenSince(uint32_t,uint32_t,uint64_t) {return false;}
bool RangeWrittenSince(uint32_t,uint32_t,uint64_t) {return false;}
bool CpuReadable(uint32_t,uint32_t) {return true;}
bool AnyGpuOwned(uint32_t,uint32_t) {return false;}
bool GpuWritePending(uint32_t,uint32_t) {return false;}
void Quiesce() {}void Watch(uint32_t,uint32_t) {}void Rearm() {}
void SetCompleted(uint64_t) {}void SetWriterFrame(uint64_t) {}
void GpuRead(uint32_t,uint32_t,uint64_t,const char*,bool) {}
void GpuWrite(uint32_t,uint32_t,uint64_t,const char*) {std::abort();}
void MarkGpuOwned(uint32_t,uint32_t) {std::abort();}
uint64_t MarkWritten(uint32_t,uint32_t,Writer,uint32_t) {std::abort();}
void SettleOpen(uint32_t,uint32_t) {std::abort();}
void GpuThreadWrite(void*,size_t,void(*write)(const void*),const void* context) {write(context);}
uint32_t WrittenPages(uint32_t,uint32_t,uint64_t,uint32_t*,uint32_t) {return 0;}
LastWrite LastWriter(uint32_t) {return {};}
GuardStats GetGuardStats() {return {};}
}
