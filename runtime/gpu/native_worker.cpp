// SpeedBreaker One. GPL-3.0-or-later.
#include <stdafx.h>
#include "native_worker.h"
#include <cpu/guest_thread.h>
#include <cpu/guest_time.h>
#include <cpu/host_cpu.h>
#include <kernel/write_watch.h>
#include <condition_variable>
#include <deque>
#include <future>
#include <functional>
#include <type_traits>
namespace gpu::native_render::worker {
namespace {
constexpr size_t MaxPending=32;
struct State {
    std::mutex mutex;
    std::condition_variable ready,space;
    std::deque<std::function<void()>> jobs;
    std::atomic<bool> running{false};
    std::atomic<uint64_t> frames{0};
    std::once_flag start;
    bool initialized=false;
};
// Renderer completion/compiler threads also live until process exit.
State& Get() {static auto* state=new State;return *state;}
thread_local bool owner=false;
void Main(std::promise<bool> result) noexcept {
    auto& state=Get();
    try {
        owner=true;writewatch::t_gpuThread=true;
        renderer::t_commandProcessor=true; // shared timing accounting
        SetHostThreadName("sb-native-render");hostcpu::UseReservedCore("native renderer");
        if(!renderer::InitializeNative()) {result.set_value(false);return;}
        state.running.store(true,std::memory_order_release);result.set_value(true);
        for(;;) {
            guesttime::WaitWhileSuspended();
            std::function<void()> job;
            {
                std::unique_lock lock(state.mutex);
                state.ready.wait_for(lock,std::chrono::microseconds(100),[&]{return !state.jobs.empty();});
                if(!state.jobs.empty()) {job=std::move(state.jobs.front());state.jobs.pop_front();}
            }
            state.space.notify_one();
            if(job) job();
            if(writewatch::g_submitRequested.exchange(false)) renderer::Submit();
            renderer::SubmitIfDue();
        }
    } catch(const std::exception& error) {
        fprintf(stderr,"[native-render] owner stopped: %s\n",error.what());
    } catch(...) {fprintf(stderr,"[native-render] owner stopped with unknown error\n");}
    state.running.store(false,std::memory_order_release);
    // Destroy queued tasks to deliver broken-promise errors to waiting callers.
    {std::lock_guard lock(state.mutex);state.jobs.clear();}
    state.ready.notify_all();state.space.notify_all();
    try {result.set_value(false);} catch(const std::future_error&) {}
}
template<class F> auto Call(F&& operation)->std::invoke_result_t<F> {
    using Result=std::invoke_result_t<F>;
    if(owner) return operation();
    if(!Start()) throw std::runtime_error("native renderer initialization failed");
    auto task=std::make_shared<std::packaged_task<Result()>>(std::forward<F>(operation));
    auto completed=task->get_future();auto& state=Get();
    {
        std::unique_lock lock(state.mutex);
        state.space.wait(lock,[&]{return !state.running.load(std::memory_order_acquire)||state.jobs.size()<MaxPending;});
        if(!state.running.load(std::memory_order_acquire)) throw std::runtime_error("native renderer owner unavailable");
        state.jobs.emplace_back([task]{(*task)();});
    }
    state.ready.notify_one();return completed.get();
}
}
bool Start() {
    auto& state=Get();std::call_once(state.start,[&]{
        std::promise<bool> result;auto ready=result.get_future();
        try {std::thread(Main,std::move(result)).detach();state.initialized=ready.get();}
        catch(const std::exception& error) {fprintf(stderr,"[native-render] start failed: %s\n",error.what());state.initialized=false;}
    });
    return state.initialized&&state.running.load(std::memory_order_acquire);
}
bool Running() {return Get().running.load(std::memory_order_acquire);}
void Draw(native_render::Draw draw) {Call([draw=std::move(draw)]{renderer::NativeDraw(draw);});}
void Clear(TargetDesc target,int32_t x,int32_t y,uint32_t width,uint32_t height,
    const float* color,float depth,uint32_t stencil,bool clearDepth,bool clearStencil) {
    std::array<float,4> owned{};bool hasColor=color!=nullptr;
    if(hasColor) std::copy(color,color+4,owned.begin());
    Call([=]{renderer::NativeClear(target,x,y,width,height,hasColor?owned.data():nullptr,depth,stencil,clearDepth,clearStencil);});
}
void Copy(TargetDesc source,TargetDesc dest,uint32_t sx,uint32_t sy,uint32_t dx,uint32_t dy,uint32_t w,uint32_t h) {
    Call([=]{renderer::NativeCopy(source,dest,sx,sy,dx,dy,w,h);});
}
void Destroy(Handle handle) {Call([=]{renderer::NativeDestroyTarget(handle);});}
std::vector<uint8_t> Readback(TargetDesc target,uint32_t x,uint32_t y,uint32_t w,uint32_t h,bool stencil) {
    return Call([=]{return renderer::NativeReadback(target,x,y,w,h,stencil);});
}
bool Present(TargetDesc source,uint32_t width,uint32_t height) {
    return Call([=]{bool shown=renderer::NativePresent(source,width,height);
        if(shown) renderer::OnSwap(Get().frames.fetch_add(1,std::memory_order_relaxed)+1);return shown;});
}
void Flush() {Call([]{renderer::Flush();});}
std::shared_future<void> Fence() {
    auto signal=std::make_shared<std::promise<void>>();auto done=signal->get_future().share();
    Call([signal]{renderer::QueueWrite([signal]{signal->set_value();});renderer::Submit();});
    return done;
}
uint64_t Frames() {return Get().frames.load(std::memory_order_acquire);}
renderer::MemoryStats GetMemoryStats() {return Call([]{return renderer::GetMemoryStats();});}
}
