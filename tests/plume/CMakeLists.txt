cmake_minimum_required(VERSION 3.24)
project(SpeedBreakerPlumeTests LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
enable_testing()
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/plume.cmake)
find_program(SB_GLSLANG NAMES glslang glslangValidator REQUIRED)
find_program(SB_SPIRV_VAL NAMES spirv-val REQUIRED)
foreach(shader triangle.vert triangle.frag words.comp)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${shader}.spv
        COMMAND ${SB_GLSLANG} -V --target-env vulkan1.2 -Os -g0 ${CMAKE_CURRENT_SOURCE_DIR}/${shader} -o ${CMAKE_CURRENT_BINARY_DIR}/${shader}.spv
        COMMAND ${SB_SPIRV_VAL} --target-env vulkan1.2 ${CMAKE_CURRENT_BINARY_DIR}/${shader}.spv
        DEPENDS ${shader})
    list(APPEND SHADERS ${CMAKE_CURRENT_BINARY_DIR}/${shader}.spv)
endforeach()
add_executable(vertex_route_fixture ../vertex_route_test.cpp)
target_include_directories(vertex_route_fixture PRIVATE ../../runtime)
target_compile_options(vertex_route_fixture PRIVATE -UNDEBUG)
add_test(NAME vertex_route_addresses COMMAND vertex_route_fixture)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/route.comp
    COMMAND vertex_route_fixture ${CMAKE_CURRENT_BINARY_DIR}/route.comp
    DEPENDS vertex_route_fixture ../../runtime/gpu/vertex_route.h)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/route.comp.spv
    COMMAND ${SB_GLSLANG} -V --target-env vulkan1.2 -Os -g0 ${CMAKE_CURRENT_BINARY_DIR}/route.comp -o ${CMAKE_CURRENT_BINARY_DIR}/route.comp.spv
    COMMAND ${SB_SPIRV_VAL} --target-env vulkan1.2 ${CMAKE_CURRENT_BINARY_DIR}/route.comp.spv
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/route.comp)
list(APPEND SHADERS ${CMAKE_CURRENT_BINARY_DIR}/route.comp.spv)
add_executable(native_draw_state_fixture ../native_draw_state_test.cpp ../../runtime/gpu/game_shader.cpp
    ../../runtime/gpu/shader_translator.cpp ../../runtime/gpu/xenos/ucode.cpp)
target_include_directories(native_draw_state_fixture PRIVATE ../../runtime)
target_compile_options(native_draw_state_fixture PRIVATE -UNDEBUG)
add_test(NAME native_draw_state COMMAND native_draw_state_fixture)
add_executable(game_shader_fixture ../game_shader_test.cpp ../../runtime/gpu/game_shader.cpp
    ../../runtime/gpu/shader_translator.cpp ../../runtime/gpu/xenos/ucode.cpp)
target_include_directories(game_shader_fixture PRIVATE ../../runtime)
target_compile_options(game_shader_fixture PRIVATE -UNDEBUG)
add_test(NAME game_shader_resources COMMAND game_shader_fixture)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/game-resource.vert
    COMMAND game_shader_fixture ${CMAKE_CURRENT_BINARY_DIR}/game-resource.vert
    DEPENDS game_shader_fixture)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/game-resource.vert.spv
    COMMAND ${SB_GLSLANG} -V --target-env vulkan1.2 -Os -g0 ${CMAKE_CURRENT_BINARY_DIR}/game-resource.vert -o ${CMAKE_CURRENT_BINARY_DIR}/game-resource.vert.spv
    COMMAND ${SB_SPIRV_VAL} --target-env vulkan1.2 ${CMAKE_CURRENT_BINARY_DIR}/game-resource.vert.spv
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/game-resource.vert)
list(APPEND SHADERS ${CMAKE_CURRENT_BINARY_DIR}/game-resource.vert.spv)
foreach(n RANGE 1 4)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/native${n}.vert.spv
        COMMAND ${SB_GLSLANG} -V --target-env vulkan1.2 -Os -g0 -DNATIVE_WORDS=${n} ${CMAKE_CURRENT_SOURCE_DIR}/native.vert -o ${CMAKE_CURRENT_BINARY_DIR}/native${n}.vert.spv
        COMMAND ${SB_SPIRV_VAL} --target-env vulkan1.2 ${CMAKE_CURRENT_BINARY_DIR}/native${n}.vert.spv
        DEPENDS native.vert)
    list(APPEND SHADERS ${CMAKE_CURRENT_BINARY_DIR}/native${n}.vert.spv)
endforeach()
add_executable(plume_offscreen_test offscreen.cpp ../../runtime/gpu/plume_backend.cpp ${SHADERS})
target_compile_definitions(plume_offscreen_test PRIVATE SB_PLUME_RHI=1 VK_NO_PROTOTYPES)
target_compile_options(plume_offscreen_test PRIVATE -UNDEBUG -include volk.h)
target_include_directories(plume_offscreen_test PRIVATE ../../runtime)
target_link_libraries(plume_offscreen_test PRIVATE plume)
add_test(NAME plume_offscreen COMMAND plume_offscreen_test ${CMAKE_CURRENT_BINARY_DIR})

add_test(NAME legacy_vulkan_fallback COMMAND plume_offscreen_test ${CMAKE_CURRENT_BINARY_DIR} legacy)
add_executable(legacy_backend_compile_check legacy_compile.cpp ../../runtime/gpu/plume_backend.cpp)
target_include_directories(legacy_backend_compile_check PRIVATE ../../runtime)
target_compile_definitions(legacy_backend_compile_check PRIVATE VK_NO_PROTOTYPES)
target_compile_options(legacy_backend_compile_check PRIVATE -UNDEBUG -include volk.h)
target_link_libraries(legacy_backend_compile_check PRIVATE volk)
add_test(NAME legacy_without_plume COMMAND legacy_backend_compile_check)
set_tests_properties(vertex_route_addresses PROPERTIES PASS_REGULAR_EXPRESSION "Vertex route source/address parity:.*words passed")
set_tests_properties(native_draw_state PROPERTIES PASS_REGULAR_EXPRESSION "Native CPU state ABI, immutable snapshots and bounds passed")
set_tests_properties(game_shader_resources PROPERTIES PASS_REGULAR_EXPRESSION "original containers, exact variants.*budget passed")
set_tests_properties(plume_offscreen legacy_vulkan_fallback PROPERTIES PASS_REGULAR_EXPRESSION "validation errors=0")
set_tests_properties(legacy_without_plume PROPERTIES PASS_REGULAR_EXPRESSION "Legacy adapter without Plume passed")
