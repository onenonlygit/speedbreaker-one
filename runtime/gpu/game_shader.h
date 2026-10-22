// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include "shader_translator.h"
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>

namespace gpu::game_shader {
// Original identity and declaration-patched executable are distinct resources.
// These bytes stay private in memory, never in a diagnostic file or asset pack.
struct Container {
    std::vector<uint8_t> bytes;
    uint32_t physicalOffset = 0, physicalBytes = 0;
    ShaderStage stage{};
};
bool ReadContainer(std::span<const uint8_t> bytes, Container& result);
bool GuestRange(uint32_t address, uint64_t size);
uint64_t Hash(uint32_t stage, std::span<const uint32_t> guestWords);
struct Resource {
    uint64_t id = 0;
    ShaderStage stage{};
    std::shared_ptr<const Container> original;
    std::vector<uint32_t> guestWords;
    TranslatedShader translated;
};
class Library {
public:
    explicit Library(size_t budget = 64 * 1024 * 1024) : budget_(budget) {}
    std::shared_ptr<const Resource> Create(uint32_t object, ShaderStage stage,
        std::shared_ptr<const Container> original, std::span<const uint32_t> guestWords);
    void Remember(uint32_t object, ShaderStage stage, std::shared_ptr<const Container> original);
    std::shared_ptr<const Container> Original(uint32_t object, ShaderStage stage);
    // Stage, length AND exact content must match. Never trust a hash alone.
    std::shared_ptr<const Resource> Find(uint32_t stage, uint64_t hash,
        std::span<const uint32_t> guestWords);
    struct Stats { uint64_t created = 0, reused = 0, hits = 0, rejected = 0; size_t bytes = 0; };
    Stats GetStats();
private:
    std::mutex mutex_;
    std::unordered_multimap<uint64_t, std::shared_ptr<const Resource>> resources_;
    std::unordered_map<uint64_t, std::shared_ptr<const Container>> objects_;
    Stats stats_;
    size_t budget_, objectBytes_ = 0;
    uint64_t nextId_ = 1;
    bool saturated_ = false;
};
Library& GetLibrary();
// Publish after all TranslateShader globals have their final values. Game
// threads acquire this before translating; they never issue Vulkan commands.
void PublishTranslationReady(bool enabled);
bool Enabled();
bool TranslationReady();
}
