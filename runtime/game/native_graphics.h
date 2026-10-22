// SpeedBreaker One. GPL-3.0-or-later.
#pragma once
#include <cstdint>
namespace gpu::native_render {class Device;}
namespace game::native_graphics {
// Resource-hook attachment only; this does not select or start a game backend.
// Attach after native worker startup and before game resource creation. The
// device must outlive attachment. Only the selected guest device's bindings
// are forwarded; resource construction/free has no guest-device argument.
void Attach(uint32_t guestDevice,gpu::native_render::Device& device);
// Drains in-progress hooks, retires tracked images and unbinds the device.
void Detach();
bool Attached();
}
