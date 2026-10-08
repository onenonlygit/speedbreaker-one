// SpeedBreaker One. GPL-3.0-or-later. Game-free platform probe only.
#include "physical_memory.h"
#include "storage.h"
#include "../atomic_ref.h"
#include <atomic>
#include <thread>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <android/log.h>
#include <vulkan/vulkan.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <signal.h>
#include <unistd.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>
namespace {
FILE* logFile = nullptr;
std::string results;
void Log(const char* fmt, ...) {
    char text[2048]; va_list ap; va_start(ap, fmt); vsnprintf(text, sizeof(text), fmt, ap); va_end(ap);
    __android_log_write(ANDROID_LOG_INFO, "SpeedBreakerOne", text);
    if (logFile) { fprintf(logFile, "%s\n", text); fflush(logFile); }
}
void Check(bool ok, const char* name) {
    Log("[%s] %s", ok ? "PASS" : "FAIL", name);
    results += std::string(ok ? "PASS: " : "FAIL: ") + name + "\n";
}
uint8_t* probe = nullptr;
volatile sig_atomic_t faultCount = 0;
void Fault(int sig, siginfo_t* info, void*) {
    auto p = reinterpret_cast<uintptr_t>(info->si_addr);
    auto start = reinterpret_cast<uintptr_t>(probe);
    if (probe && p >= start && p < start + 4096 && mprotect(probe, 4096, PROT_READ | PROT_WRITE) == 0) {
        faultCount = faultCount + 1; return;
    }
    _exit(128 + sig);
}
bool AtomicProbe() {
    alignas(4) uint32_t word = 1;
    platform::AtomicRef ref(word);
    std::atomic<bool> entered{false}, resumed{false};
    std::thread waiter([&] { entered.store(true); ref.wait(1); resumed.store(ref.load() == 0); });
    while (!entered.load()) std::this_thread::yield();
    ref.store(0); ref.notify_one(); waiter.join();
    uint32_t expected = 0;
    bool cas = ref.compare_exchange_strong(expected, 7);
    expected = 2;
    bool mismatch = !ref.compare_exchange_strong(expected, 8) && expected == 7;
    return resumed.load() && cas && mismatch;
}
bool MemoryProbe() {
    constexpr size_t size = 0x100000000ULL;
    auto* base = static_cast<uint8_t*>(mmap(reinterpret_cast<void*>(size), size,
        PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
    Check(base != MAP_FAILED, "4 GiB guest reservation");
    if (base == MAP_FAILED) return false;
    uint32_t shift = 0;
    bool mapped = platform::android::MapPhysicalMemory(base, shift);
    Check(mapped, "512 MiB shared physical backing / three windows");
    if (!mapped) { munmap(base, size); return false; }
    auto* a = reinterpret_cast<volatile uint32_t*>(base + 0xA0001000ULL);
    auto* c = reinterpret_cast<volatile uint32_t*>(base + 0xC0001000ULL);
    auto* e = reinterpret_cast<volatile uint32_t*>(base + 0xE0000000ULL);
    *a = 0x13579bdf;
    bool aliases = *c == *a && *e == *a && shift == 4096;
    *e = 0x2468ace0;
    aliases = aliases && *a == *e && *c == *e;
    Check(aliases, "bidirectional A/C/E aliases and exact +4 KiB E shift");
    struct sigaction sa{}, old{}; sa.sa_sigaction = Fault; sa.sa_flags = SA_SIGINFO; sigemptyset(&sa.sa_mask);
    bool signals = sigaction(SIGSEGV, &sa, &old) == 0;
    probe = base + 0xA0001000ULL;
    bool protectedWrite = signals && mprotect(probe, 4096, PROT_READ) == 0;
    if (protectedWrite) *a = 0xfeed1234;
    bool protectedRead = protectedWrite && mprotect(probe, 4096, PROT_NONE) == 0;
    volatile uint32_t value = protectedRead ? *a : 0;
    bool protection = protectedRead && faultCount == 2 && value == 0xfeed1234 && *c == value && *e == value;
    if (signals) sigaction(SIGSEGV, &old, nullptr);
    probe = nullptr;
    Check(protection, "mprotect read/write faults, SIGSEGV retry, alias visibility");
    // This probes kernel primitives, not the entire runtime write-watch algorithm.
    munmap(base, size);
    return aliases && protection;
}
bool VulkanProbe(SDL_Window* window) {
    Uint32 extCount = 0;
    auto extensions = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!extensions) { Log("SDL Vulkan extensions: %s", SDL_GetError()); return false; }
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = extCount; ci.ppEnabledExtensionNames = extensions;
    VkInstance instance = VK_NULL_HANDLE; VkSurfaceKHR surface = VK_NULL_HANDLE;
    bool ok = vkCreateInstance(&ci, nullptr, &instance) == VK_SUCCESS;
    Check(ok, "Vulkan 1.2 instance"); if (!ok) return false;
    ok = SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);
    Check(ok, "Android Vulkan surface via SDL3");
    if (ok) {
        uint32_t count = 0; vkEnumeratePhysicalDevices(instance, &count, nullptr);
        std::vector<VkPhysicalDevice> devices(count); vkEnumeratePhysicalDevices(instance, &count, devices.data());
        bool found = false;
        for (auto d : devices) {
            VkPhysicalDeviceProperties p{}; vkGetPhysicalDeviceProperties(d, &p);
            Log("GPU=%s driver=%u API=%u.%u.%u", p.deviceName, p.driverVersion,
                VK_VERSION_MAJOR(p.apiVersion), VK_VERSION_MINOR(p.apiVersion), VK_VERSION_PATCH(p.apiVersion));
            uint32_t n = 0; vkGetPhysicalDeviceQueueFamilyProperties(d, &n, nullptr);
            std::vector<VkQueueFamilyProperties> queues(n); vkGetPhysicalDeviceQueueFamilyProperties(d, &n, queues.data());
            for (uint32_t q = 0; q < n; q++) { VkBool32 present = false; vkGetPhysicalDeviceSurfaceSupportKHR(d, q, surface, &present);
                found |= bool(present && (queues[q].queueFlags & VK_QUEUE_GRAPHICS_BIT)); }
            VkPhysicalDeviceFeatures f{}; vkGetPhysicalDeviceFeatures(d, &f);
            Log("BC compression=%u shaderInt16=%u dualSrcBlend=%u", f.textureCompressionBC, f.shaderInt16, f.dualSrcBlend);
            uint32_t en = 0; vkEnumerateDeviceExtensionProperties(d, nullptr, &en, nullptr);
            std::vector<VkExtensionProperties> available(en); vkEnumerateDeviceExtensionProperties(d, nullptr, &en, available.data());
            for (const char* required : {"VK_KHR_swapchain", "VK_KHR_dynamic_rendering", "VK_KHR_depth_stencil_resolve", "VK_KHR_create_renderpass2", "VK_KHR_multiview", "VK_KHR_maintenance2", "VK_KHR_push_descriptor"}) {
                bool has = false; for (const auto& x : available) has |= strcmp(x.extensionName, required) == 0;
                Log("renderer extension %s: %s", required, has ? "yes" : "MISSING");
            }
        }
        Check(found, "graphics queue supports Android presentation"); ok = found;
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    vkDestroyInstance(instance, nullptr); return ok;
}
}
int main(int, char**) {
    auto file = platform::android::Files() / "android-diagnostics.log";
    logFile = fopen(file.c_str(), "w");
    Log("SpeedBreaker One 0.1.0 Android platform diagnostics; NO GAME CODE; page size=%ld", sysconf(_SC_PAGESIZE));
    struct sysinfo info{}; if (sysinfo(&info) == 0) Log("RAM total=%llu free=%llu bytes", (unsigned long long)info.totalram * info.mem_unit, (unsigned long long)info.freeram * info.mem_unit);
    Check(MemoryProbe(), "guest-memory primitive probe complete");
    Check(AtomicProbe(), "guest word atomic CAS / futex wait and notify");
    bool sdl = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO);
    Check(sdl, "SDL video/gamepad/audio initialization");
    if (!sdl) { Log("SDL error: %s", SDL_GetError()); if (logFile) fclose(logFile); return 1; }
    SDL_Window* window = SDL_CreateWindow("SpeedBreaker One — diagnostics", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN);
    Check(window != nullptr, "fullscreen Vulkan window");
    if (window) Check(VulkanProbe(window), "Vulkan surface capability probe complete");
    int count = 0; SDL_JoystickID* ids = SDL_GetGamepads(&count);
    std::vector<SDL_Gamepad*> pads;
    for (int i = 0; i < count; i++) { auto* pad = SDL_OpenGamepad(ids[i]); if (pad) { pads.push_back(pad); Log("Controller: %s", SDL_GetGamepadName(pad)); } }
    SDL_free(ids); int audioCount = 0; auto* audioIds = SDL_GetAudioPlaybackDevices(&audioCount);
    Log("gamepads=%zu; audio playback devices=%d", pads.size(), audioCount); SDL_free(audioIds);
    results += "\nThis is a platform probe, not the playable game.\nResults are in android-diagnostics.log and logcat tag SpeedBreakerOne.\nClose this dialog, then press buttons to log controller input.\nAndroid Back exits.";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "SpeedBreaker One — platform diagnostics", results.c_str(), window);
    bool running = true;
    while (running && window) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch(e.type) {
            case SDL_EVENT_QUIT: case SDL_EVENT_TERMINATING: running = false; break;
            case SDL_EVENT_KEY_DOWN: if (e.key.key == SDLK_AC_BACK) running = false; break;
            case SDL_EVENT_GAMEPAD_ADDED: { auto* pad = SDL_OpenGamepad(e.gdevice.which); if (pad) { pads.push_back(pad); Log("Controller added: %s", SDL_GetGamepadName(pad)); } break; }
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN: Log("button gamepad=%u button=%u", e.gbutton.which, e.gbutton.button); break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION: Log("axis gamepad=%u axis=%u value=%d", e.gaxis.which, e.gaxis.axis, e.gaxis.value); break;
            case SDL_EVENT_WILL_ENTER_BACKGROUND: Log("lifecycle: background"); break;
            case SDL_EVENT_DID_ENTER_FOREGROUND: Log("lifecycle: foreground"); break;
            default: break;
            }
        }
        SDL_Delay(20);
    }
    for (auto* p : pads) SDL_CloseGamepad(p);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit(); if (logFile) fclose(logFile); return 0;
}
