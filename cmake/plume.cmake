# Pinned MIT Plume plus a small, reviewable shared-device interoperability patch.
include(FetchContent)
set(PLUME_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PLUME_SDL2_VULKAN_ENABLED OFF CACHE BOOL "" FORCE)
set(PLUME_SDL3_VULKAN_ENABLED OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sb_plume
    GIT_REPOSITORY https://github.com/renderbag/plume.git
    GIT_TAG e48a55be0cff0f1e82b25cd0ab68be37357ee0f9
    GIT_SHALLOW FALSE
    GIT_SUBMODULES contrib/volk contrib/Vulkan-Headers contrib/VulkanMemoryAllocator
    GIT_SUBMODULES_RECURSE TRUE
    PATCH_COMMAND ${CMAKE_COMMAND} -DSOURCE=<SOURCE_DIR>
        -DPATCH=${CMAKE_CURRENT_LIST_DIR}/../patches/plume/0001-shared-vulkan-interop.patch
        -DPATCH2=${CMAKE_CURRENT_LIST_DIR}/../patches/plume/0002-borrowed-vulkan-buffer.patch
        -DPATCH3=${CMAKE_CURRENT_LIST_DIR}/../patches/plume/0003-borrowed-pipeline-debug-construction.patch
        -P ${CMAKE_CURRENT_LIST_DIR}/apply_plume_patch.cmake)
FetchContent_MakeAvailable(sb_plume)
# Also upgrade existing populated checkouts when the patch series grows.
execute_process(COMMAND ${CMAKE_COMMAND} -DSOURCE=${sb_plume_SOURCE_DIR}
    -DPATCH=${CMAKE_CURRENT_LIST_DIR}/../patches/plume/0001-shared-vulkan-interop.patch
    -DPATCH2=${CMAKE_CURRENT_LIST_DIR}/../patches/plume/0002-borrowed-vulkan-buffer.patch
    -DPATCH3=${CMAKE_CURRENT_LIST_DIR}/../patches/plume/0003-borrowed-pipeline-debug-construction.patch
    -P ${CMAKE_CURRENT_LIST_DIR}/apply_plume_patch.cmake COMMAND_ERROR_IS_FATAL ANY)
# Plume's single translation unit normally defines VOLK_IMPLEMENTATION itself.
# SpeedBreaker owns initialization from the selected libadrenotools/system handle.
target_compile_definitions(plume PRIVATE PLUME_EXTERNAL_VOLK VMA_STATIC_VULKAN_FUNCTIONS=0)
set(VOLK_PULL_IN_VULKAN OFF)
set(VOLK_STATIC_DEFINES "VK_ENABLE_BETA_EXTENSIONS")
if(ANDROID)
    list(APPEND VOLK_STATIC_DEFINES VK_USE_PLATFORM_ANDROID_KHR)
endif()
add_subdirectory(${sb_plume_SOURCE_DIR}/contrib/volk ${CMAKE_BINARY_DIR}/plume-volk)
target_include_directories(volk PUBLIC ${sb_plume_SOURCE_DIR}/contrib/Vulkan-Headers/include)
set_target_properties(volk PROPERTIES C_VISIBILITY_PRESET hidden)
target_link_libraries(plume PUBLIC volk)

if(UNIX AND NOT ANDROID AND NOT APPLE)
    target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_XLIB_KHR)
    find_library(PLUME_X11_LIBRARY NAMES X11 libX11.so.6 REQUIRED)
    target_link_libraries(plume PUBLIC ${PLUME_X11_LIBRARY})
endif()
