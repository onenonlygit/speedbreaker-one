// SpeedBreaker runtime. GPL-3.0-or-later (see COPYING).
// Synthetic Xenos exports exercise the actual translator without game data.
#include <gpu/shader_translator.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

static std::vector<uint32_t> Exports(const std::vector<uint32_t>& destinations)
{
    const uint32_t count = uint32_t(destinations.size());
    const uint32_t firstCount = count > 6 ? 6 : count;
    uint32_t a0 = 1 | (firstCount << 12);
    uint32_t a1 = (count > 6 ? 1u : 2u) << 12; // Exec / ExecEnd
    uint32_t b0 = count > 6 ? 7 | ((count - 6) << 12) : 0;
    uint32_t b1 = count > 6 ? 2u << 12 : 0;
    std::vector<uint32_t> words{a0, a1 | (b0 << 16), (b0 >> 16) | (b1 << 16)};
    for (auto dest : destinations)
    {
        // Vector MAX r0,r0, exported with all components; scalar RetainPrev.
        words.push_back(dest | (1u << 15) | (15u << 16) | (50u << 26));
        words.push_back(0);
        words.push_back((2u << 24) | (1u << 31) | (1u << 30));
    }
    return words;
}

int main(int argc, char** argv)
{
    auto vsWords = Exports({62, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    auto psWords = Exports({0});
    auto vs = gpu::TranslateShader(gpu::ShaderStage::Vertex, vsWords.data(), vsWords.size());
    auto ps = gpu::TranslateShader(gpu::ShaderStage::Pixel, psWords.data(), psWords.size());
    assert(vs.ok && ps.ok);
    assert(vs.interpolatorMask == 0x7ff);
    assert(ps.colorTargetMask == 1);
    assert(vs.glsl.find("@@") == std::string::npos);
    assert(ps.glsl.find("@@") == std::string::npos);
    if (argc == 2)
    {
        std::filesystem::create_directories(argv[1]);
        for (unsigned mode = 0; mode != 3; ++mode)
        {
            std::ofstream out(std::filesystem::path(argv[1]) / ("mode" + std::to_string(mode) + ".vert"));
            auto source = vs.glsl;
            auto pos = source.find('\n');
            source.insert(pos + 1, "#define VERTEX_PRIMITIVE_MODE " + std::to_string(mode) + "\n");
            out << source;
            assert(out.good());
        }
        auto source = ps.glsl;
        source.insert(source.find('\n') + 1, "#define OUT0 1\n");
        std::ofstream out(std::filesystem::path(argv[1]) / "pixel.frag");
        out << source;
        assert(out.good());
    }
    std::cout << "Synthetic shader translation and variant generation passed\n";
}
