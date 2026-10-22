// No Plume dependency in this target: includes stale saved preference recovery.
#include <gpu/plume_backend.h>
#include <cassert>
#include <cstdio>
int main() {
    assert(gpu::plume_rhi::Initialize(VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,0,false));
    assert(!gpu::plume_rhi::Enabled());
    assert(gpu::plume_rhi::Initialize(VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,0,true));
    assert(!gpu::plume_rhi::Enabled());
    gpu::plume_rhi::Shutdown();
    puts("Legacy adapter without Plume passed");
}
