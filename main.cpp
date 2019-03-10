#include <memory>

#include <radeon_rays.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMPLEMENTATION
#include <stb_image.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>

#define GLFW_INCLUDE_GLU
#include <GLFW/glfw3.h>

using RadeonRays::DeviceInfo;
using RadeonRays::IntersectionApi;

struct IntersectionApiDeleter {
  void operator()(IntersectionApi *p) { IntersectionApi::Delete(p); }
};

using IntersectionApiPtr = std::unique_ptr<IntersectionApi, IntersectionApiDeleter>;

int main()
{
  int deviceidx = -1;
  for (auto idx = 0U; idx < IntersectionApi::GetDeviceCount(); ++idx) {
    DeviceInfo devinfo;
    IntersectionApi::GetDeviceInfo(idx, devinfo);

    if (devinfo.type == DeviceInfo::kGpu) {
      deviceidx = idx;
    }
  }

  IntersectionApi::SetPlatform(DeviceInfo::kOpenCL);

  auto isapi = IntersectionApiPtr(IntersectionApi::Create(deviceidx));

  return 0;
}
