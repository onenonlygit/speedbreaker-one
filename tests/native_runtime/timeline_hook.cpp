// Verify real wrapper linkage, original-call count and complete context return.
#include <stdafx.h>
#include <cpu/guest_time.h>
#include <debug/timeline.h>
namespace guesttime {State g_state;}
static unsigned swaps=0,waits=0;
static void Result(PPCContext& ctx) {ctx.r3.u64=0x123456789abcdef0ull;ctx.r4.u64^=0xa5a5;ctx.lr^=0x1234;}
extern "C" PPC_FUNC(__imp__sub_825987E0) {++swaps;Result(ctx);}
extern "C" PPC_FUNC(__imp__sub_82597498) {++waits;Result(ctx);}
PPC_FUNC(sub_825987E0);
PPC_FUNC(sub_82597498);
int main() {
    timeline::g_enabled=false;
    PPCContext input{},expected{},actual{};input.r4.u64=0x9876;input.lr=0x8888;
    memcpy(&expected,&input,sizeof(input));Result(expected);
    memcpy(&actual,&input,sizeof(input));sub_825987E0(actual,nullptr);
    assert(swaps==1&&waits==0&&!memcmp(&actual,&expected,sizeof(actual)));
    memcpy(&actual,&input,sizeof(input));sub_82597498(actual,nullptr);
    assert(swaps==1&&waits==1&&!memcmp(&actual,&expected,sizeof(actual)));
    assert(!timeline::t_waitTargetMarked);
}
