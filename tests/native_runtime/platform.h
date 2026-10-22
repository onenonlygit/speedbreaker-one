#pragma once
#include <volk.h>
#include <vulkan/vulkan.h>
// Xlib macros conflict with game types after including Plume platform headers.
#ifdef Bool
#undef Bool
#endif
#ifdef None
#undef None
#endif
#ifdef Status
#undef Status
#endif
