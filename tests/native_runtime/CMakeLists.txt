cmake_minimum_required(VERSION 3.24)
project(NativeRuntimeTest LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
enable_testing()
get_filename_component(ROOT ../.. ABSOLUTE BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
include(${ROOT}/cmake/plume.cmake)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(${ROOT}/tools/glslang-android glslang)
add_executable(native_runtime_test main.cpp stubs.cpp
    ${ROOT}/runtime/gpu/renderer.cpp ${ROOT}/runtime/gpu/plume_backend.cpp
    ${ROOT}/runtime/gpu/native_worker.cpp ${ROOT}/runtime/gpu/game_shader.cpp
    ${ROOT}/runtime/gpu/shader_translator.cpp ${ROOT}/runtime/gpu/xenos/ucode.cpp)
target_compile_definitions(native_runtime_test PRIVATE SB_PLUME_RHI=1 VK_NO_PROTOTYPES)
target_compile_options(native_runtime_test PRIVATE -UNDEBUG -include ${CMAKE_CURRENT_SOURCE_DIR}/platform.h
    -ffunction-sections -fdata-sections -fno-strict-aliasing)
target_include_directories(native_runtime_test PRIVATE ${ROOT}/runtime ${ROOT}/runtime/kernel
    ${ROOT}/runtime/thirdparty/o1heap ${ROOT}/ppc ${ROOT}/tools/XenonRecomp/XenonUtils
    ${ROOT}/tools/XenonRecomp/thirdparty/simde ${ROOT}/tools/XenonRecomp/thirdparty/xxHash)
target_link_options(native_runtime_test PRIVATE -Wl,--gc-sections)
target_link_libraries(native_runtime_test PRIVATE plume glslang::glslang glslang::SPIRV
    glslang::glslang-default-resource-limits)
option(SB_NATIVE_SANITIZE "AddressSanitizer" OFF)
if(SB_NATIVE_SANITIZE)
    target_compile_options(native_runtime_test PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(native_runtime_test PRIVATE -fsanitize=address)
endif()
foreach(threads RANGE 0 1)
    add_test(NAME native_runtime_${threads} COMMAND native_runtime_test)
    set_tests_properties(native_runtime_${threads} PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE
        ENVIRONMENT "NFSMW_PIPELINE_THREADS=${threads};NFSMW_IMAGE_POOL=0;NFSMW_LOG_UNTILED_AREA=1")
endforeach()
