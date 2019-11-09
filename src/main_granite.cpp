#include <cassert>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <muglm/muglm.hpp>

#include <tiny_obj_loader.h>

#include <bvh/bvh_builder.h>

#include "os_filesystem.hpp"
#include <granite/application/application.hpp>
#include <granite/renderer/render_graph.hpp>
#include <granite/vulkan/command_buffer.hpp>

using namespace Granite;
using namespace Vulkan;

#pragma pack(push, 1)

struct DebugVars {
  float debug_var_cam_offset;
  float debug_var_scale;
  int debug_var_int_1;
  float debug_var_float_1;
  float debug_var_float_2;
};

struct ViewData {
  muglm::mat4 view_transform;
  muglm::vec3 eye_pos;
  int32_t _pad1[1];
  float image_size_norm[2];
  int32_t _pad2[2];
};

#pragma pack(pop)

struct RenderGraphSandboxApplication : Granite::Application, Granite::EventHandler {
  RenderGraphSandboxApplication() {
    EVENT_MANAGER_REGISTER_LATCH(
      RenderGraphSandboxApplication, on_swapchain_created, on_swapchain_destroyed,
      SwapchainParameterEvent);
  }

  void on_swapchain_created(const SwapchainParameterEvent &e) {
    auto &device = e.get_device();

    graph.reset();
    graph.set_device(&device);

    ResourceDimensions dim;
    dim.width = e.get_width();
    dim.height = e.get_height();
    dim.format = e.get_format();
    graph.set_backbuffer_dimensions(dim);

    // Common buffers
    BufferHandle view_data_buffer = {};
    {
      ViewData view_data = {}; // TODO

      BufferCreateInfo info = {};
      info.size = sizeof(view_data);
      info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      view_data_buffer = device.create_buffer(info, &view_data);
    }

    AttachmentInfo back;

    auto &pass_graphics = graph.add_pass("graphics", RENDER_GRAPH_QUEUE_GRAPHICS_BIT);
    auto &rt_back = pass_graphics.add_color_output("back", back);
    pass_graphics.set_get_clear_color([](unsigned, VkClearColorValue *value) -> bool {
      if (value) {
        value->float32[0] = 1.0f;
        value->float32[1] = 0.0f;
        value->float32[2] = 1.0f;
        value->float32[3] = 1.0f;
      }
      return true;
    });

    pass_graphics.set_build_render_pass([&](Vulkan::CommandBuffer &cmd) {
      // CommandBufferUtil::setup_fullscreen_quad(cmd, "builtin://shaders/quad.vert", "shaders://depth.frag");
      CommandBufferUtil::setup_fullscreen_quad(
        cmd, "builtin://shaders/quad.vert", "assets://shaders/additive.frag");
      cmd.set_blend_enable(true);
      cmd.set_blend_factors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);

      // cmd.set_storage_buffer(0, 4, *view_data_buffer);
      //{
      //  DebugVars debug_vars = {}; // TODO
      //  cmd.push_constants(&debug_vars, 0, sizeof(debug_vars));
      //}

      CommandBufferUtil::draw_fullscreen_quad(cmd, 80);
    });

    graph.set_backbuffer_source("back");
    graph.bake();
    graph.log();

#if 0
    AttachmentInfo im;
    im.format = VK_FORMAT_R8G8B8A8_UNORM;
    im.size_x = 1280.0f;
    im.size_y = 720.0f;
    im.size_class = SizeClass::Absolute;

    // Pretend depth pass.
    auto &depth = graph.add_pass("depth", RENDER_GRAPH_QUEUE_GRAPHICS_BIT);
    depth.add_color_output("depth", back);

    depth.set_get_clear_color([](unsigned, VkClearColorValue *value) -> bool {
      if (value) {
        value->float32[0] = 0.0f;
        value->float32[1] = 1.0f;
        value->float32[2] = 0.0f;
        value->float32[3] = 1.0f;
      }
      return true;
    });

    depth.set_build_render_pass([&](Vulkan::CommandBuffer &cmd) {
      CommandBufferUtil::setup_fullscreen_quad(
        cmd, "builtin://shaders/quad.vert", "assets://shaders/additive.frag");
      cmd.set_blend_enable(true);
      cmd.set_blend_factors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
      CommandBufferUtil::draw_fullscreen_quad(cmd, 20);
    });

    // Pretend main rendering pass.
    auto &graphics = graph.add_pass("first", RENDER_GRAPH_QUEUE_GRAPHICS_BIT);
    auto &first = graphics.add_color_output("first", back);
    graphics.add_texture_input("depth");
    graphics.set_get_clear_color([](unsigned, VkClearColorValue *value) -> bool {
      if (value) {
        value->float32[0] = 1.0f;
        value->float32[1] = 0.0f;
        value->float32[2] = 1.0f;
        value->float32[3] = 1.0f;
      }
      return true;
    });

    graphics.set_build_render_pass([&](Vulkan::CommandBuffer &cmd) {
      CommandBufferUtil::setup_fullscreen_quad(
        cmd, "builtin://shaders/quad.vert", "assets://shaders/additive.frag");
      cmd.set_blend_enable(true);
      cmd.set_blend_factors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
      CommandBufferUtil::draw_fullscreen_quad(cmd, 80);
    });

    // Post processing
    auto &compute = graph.add_pass("compute", RENDER_GRAPH_QUEUE_ASYNC_COMPUTE_BIT);
    auto &i = compute.add_storage_texture_output("image", im);
    compute.add_texture_input("first");
    compute.set_build_render_pass([&](Vulkan::CommandBuffer &cmd) {
      auto &device = get_wsi().get_device();
      auto *program =
        device.get_shader_manager().register_compute("assets://shaders/image_write.comp");
      unsigned variant = program->register_variant({});
      cmd.set_program(program->get_program(variant));
      cmd.set_storage_texture(0, 0, graph.get_physical_texture_resource(i));
      cmd.set_texture(0, 1, graph.get_physical_texture_resource(first), StockSampler::LinearClamp);
      cmd.dispatch(1280 / 8, 720 / 8, 40);
    });

    // Composite + UI
    auto &swap = graph.add_pass("final", RENDER_GRAPH_QUEUE_ASYNC_GRAPHICS_BIT);
    swap.add_color_output("back", back);
    swap.add_texture_input("image");
    swap.add_texture_input("first");
    swap.set_build_render_pass([&](Vulkan::CommandBuffer &cmd) {
      cmd.set_texture(0, 0, graph.get_physical_texture_resource(i), StockSampler::LinearClamp);
      cmd.set_texture(0, 1, graph.get_physical_texture_resource(first), StockSampler::LinearClamp);
      CommandBufferUtil::draw_fullscreen_quad(
        cmd, "builtin://shaders/quad.vert", "builtin://shaders/blit.frag");
    });

    graph.set_backbuffer_source("back");
    graph.bake();
    graph.log();
#endif
  }

  void on_swapchain_destroyed(const SwapchainParameterEvent &) {}

  void render_frame(double, double) {
    auto &wsi = get_wsi();
    auto &device = wsi.get_device();
    graph.setup_attachments(device, &device.get_swapchain_view());
    graph.enqueue_render_passes(device);
  }

  RenderGraph graph;
};

namespace Granite {
Application *application_create(int argc, char **argv) {
#if defined(_WIN32) && defined(ENABLE_WINDOWS_CONSOLE)
  if (AllocConsole()) {
    FILE *pCout;
    freopen_s(&pCout, "CONOUT$", "w", stdout);
    FILE *pCerr;
    freopen_s(&pCerr, "CONOUT$", "w", stderr);
  }
#endif

  application_dummy();

  // Register filesystem protocols.

#define CACHE_DIRECTORY "cache"
  Global::filesystem()->register_protocol(
    "cache", std::unique_ptr<FilesystemBackend>(new OSFilesystem(CACHE_DIRECTORY)));

#define BUILTIN_DIRECTORY "third_party/granite/assets"
  Global::filesystem()->register_protocol(
    "builtin", std::unique_ptr<FilesystemBackend>(new OSFilesystem(BUILTIN_DIRECTORY)));

#define ASSET_DIRECTORY "third_party/granite/tests/assets"
#ifdef ASSET_DIRECTORY
  const char *asset_dir = getenv("ASSET_DIRECTORY");
  if (!asset_dir)
    asset_dir = ASSET_DIRECTORY;

  Global::filesystem()->register_protocol(
    "assets", std::unique_ptr<FilesystemBackend>(new OSFilesystem(asset_dir)));
#endif


  Global::filesystem()->register_protocol(
    "models", std::unique_ptr<FilesystemBackend>(new OSFilesystem("../models")));
  Global::filesystem()->register_protocol(
    "shaders", std::unique_ptr<FilesystemBackend>(new OSFilesystem("shaders")));

  // Load geometry.
  std::unique_ptr<RadeonRays::Shape> test_mesh;
  {
    printf("Loading geometry\n");

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;
    const std::string filename = "../assets/lucy/lucy_watertight.obj";
    // const std::string filename = "../models/teapot.obj";
    // const std::string filename = "../models/icosahedron.obj";
    const bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str());

    assert(!shapes.empty());

    const auto &mesh = shapes[0].mesh;

    //// DEBUG ////
    // Scale model into unit cube.
    {
      RadeonRays::bbox bbox;
      auto &vs = attrib.vertices;
      for (int i = 0; i < vs.size(); i += 3) {
        bbox.grow({ vs[i], vs[i + 1], vs[i + 2] });
      }
      const auto center = bbox.center();
      const auto extent = bbox.extents();
      const float scale = std::max(extent.x, std::max(extent.y, extent.z));
      for (int i = 0; i < vs.size(); i += 3) {
        vs[i + 0] = (vs[i + 0] - center.x) / scale;
        vs[i + 1] = (vs[i + 1] - center.y) / scale;
        vs[i + 2] = (vs[i + 2] - center.z) / scale;
      }
    }
    ///////////////

    std::vector<int> indices;
    indices.reserve(mesh.indices.size());
    for (const auto &tinyobj_index : mesh.indices) {
      indices.push_back(tinyobj_index.vertex_index);
    }

    std::vector<int> face_vertex_counts(mesh.num_face_vertices.begin(), mesh.num_face_vertices.end());

    const auto &vertices = attrib.vertices;

    const int vertex_size_bytes = 3 * int(sizeof(float));
    const int num_vertices = int(vertices.size()) / 3;
    const int num_faces = int(face_vertex_counts.size());

    test_mesh = std::make_unique<RadeonRays::Mesh>(
      vertices.data(), num_vertices, vertex_size_bytes, indices.data(), 0,
      face_vertex_counts.data(), num_faces);
    test_mesh->SetId(1);
  }

  // Create the app object.

  try {
    auto *app = new RenderGraphSandboxApplication();
    return app;
  } catch (const std::exception &e) {
    LOGE("application_create() threw exception: %s\n", e.what());
    return nullptr;
  }
}
} // namespace Granite
