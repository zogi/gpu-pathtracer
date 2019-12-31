#include <cassert>
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include <muglm/muglm.hpp>
#include <muglm/muglm_impl.hpp>

#include <tiny_obj_loader.h>

#include <bvh/bvh_builder.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "os_filesystem.hpp"
#include <granite/application/application.hpp>
#include <granite/ecs/ecs.hpp>
#include <granite/renderer/camera.hpp>
#include <granite/renderer/mesh_util.hpp>
#include <granite/renderer/render_context.hpp>
#include <granite/renderer/render_graph.hpp>
#include <granite/renderer/renderer.hpp>
#include <granite/renderer/scene_loader.hpp>
#include <granite/util/util.hpp>
#include <granite/vulkan/command_buffer.hpp>

//#include "camera.h"

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

struct ViewUniforms {
  muglm::mat4 view_transform;
  muglm::vec3 eye_pos;
  int32_t _pad1[1];
  float image_size_norm[2];
  int32_t _pad2[2];
};

#pragma pack(pop)

// Data bound to the lifetime of a "view" - subinterval of a frame that
// corresponds to rendering objects from a particular point of view.
struct ViewData {
  ViewUniforms view_uniforms;
};

// Data bound to the lifetime of a frame.
struct FrameData {
  DebugVars debug_vars;
};

// Data bound to the lifetime of a swapchain (between window resize events).
struct SwapchainData {
  muglm::ivec2 window_size = {};
};

// CPU data bound to the lifetime of a scene.
struct SceneData {
  std::unique_ptr<RadeonRays::Shape> test_shape;

  // Camera camera_;
  // CameraController camera_controller_;
  Granite::FPSCamera cam;
};

// Data bound to the lifetime of a GPU device.
struct DeviceData {
  BufferHandle view_data_buffer;

  BufferHandle bvh_nodes_buffer;
  BufferHandle bvh_vtx_buffer;
  BufferHandle bvh_faces_buffer;
};

struct GlobalData {
  ViewData view_data; // TODO: array?
  FrameData frame_data; // TODO: array?
  SwapchainData swapchain_data;
  SceneData scene_data;
  DeviceData device_data;
};

void init_device_data(Device &device, DeviceData &device_data, const SceneData &scene_data) {
  // Build BVH.
  {
    const RadeonRays::Shape *shape = scene_data.test_shape.get();

    assert(shape != nullptr);

    RadeonRays::World world;
    world.AttachShape(shape);
    world.OnCommit();

    // Use surface area heuristic for better intersection performance (but
    // slower scene build time).
    world.options_.SetValue("bvh.builder", "sah");

    LOGI("Building BVH");

    RadeonRays::BvhBuilder builder;
    builder.updateBvh(world);

    std::vector<RadeonRays::Node> nodes(builder.getNodeCount());
    std::vector<RadeonRays::Vertex> verts(builder.getVertexCount());
    std::vector<RadeonRays::Face> faces(builder.getFaceCount());
    builder.fillBuffers(nodes, verts, faces);

    const auto create_bvh_buffer = [&](size_t size, void *data) {
      constexpr VkBufferUsageFlagBits kDebugFlag = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      BufferCreateInfo info = {};
      info.size = size;
      info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | kDebugFlag;
      return device.create_buffer(info, data);
    };
    BufferHandle bvh_nodes_buffer = create_bvh_buffer(builder.getNodeBufferSizeBytes(), nodes.data());
    BufferHandle bvh_vtx_buffer = create_bvh_buffer(builder.getVertexBufferSizeBytes(), verts.data());
    BufferHandle bvh_faces_buffer = create_bvh_buffer(builder.getFaceBufferSizeBytes(), faces.data());

    device_data.bvh_nodes_buffer = std::move(bvh_nodes_buffer);
    device_data.bvh_vtx_buffer = std::move(bvh_vtx_buffer);
    device_data.bvh_faces_buffer = std::move(bvh_faces_buffer);
  }

  // Common buffers
  {
    BufferCreateInfo info = {};
    info.domain = Vulkan::BufferDomain::LinkedDeviceHost;
    info.size = sizeof(ViewData);
    info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    auto view_data_buffer = device.create_buffer(info, nullptr);

    device_data.view_data_buffer = std::move(view_data_buffer);
  }
}

void delete_device_data(DeviceData &device_data, Device &device) {
  device_data.view_data_buffer.reset();
  device_data.bvh_nodes_buffer.reset();
  device_data.bvh_vtx_buffer.reset();
  device_data.bvh_faces_buffer.reset();
}

struct RenderGraphSandboxApplication : Granite::Application, Granite::EventHandler {
  RenderGraphSandboxApplication(const std::string &scene_path)
    : renderer_(RendererType::GeneralForward) {
    // Set up event handlers.
    EVENT_MANAGER_REGISTER_LATCH(
      RenderGraphSandboxApplication, on_device_created, on_device_destroyed, DeviceCreatedEvent);
    EVENT_MANAGER_REGISTER_LATCH(
      RenderGraphSandboxApplication, on_swapchain_created, on_swapchain_destroyed,
      SwapchainParameterEvent);

    // Load scene.
    scene_loader_.load_scene(scene_path);

    // Set up real-time rendering context.
    lighting_.directional.color = vec3(1, 1, 1);
    lighting_.directional.direction = normalize(vec3(0.5f, 1.2f, 0.8f));
    context_.set_lighting_parameters(&lighting_);

    cam_.set_depth_range(0.1f, 1000.0f);
    cam_.look_at(vec3(-1.0f, 0.0f, 0.0f), vec3(1.0f, 0.0f, 0.0f));
    context_.set_camera(cam_);
  }

  void on_device_created(const DeviceCreatedEvent &e) {
    //
    init_device_data(e.get_device(), global_data_.device_data, global_data_.scene_data);
  }

  void on_device_destroyed(const DeviceCreatedEvent &e) {
    //
    delete_device_data(global_data_.device_data, e.get_device());
  }

  void on_swapchain_created(const SwapchainParameterEvent &e) {
    auto &device = e.get_device();

    // Update extent in SceneData
    global_data_.swapchain_data.window_size = { int32_t(e.get_width()), int32_t(e.get_height()) };

    // Rebuild render graph.

    graph_.reset();
    graph_.set_device(&device);

    ResourceDimensions dim;
    dim.width = e.get_width();
    dim.height = e.get_height();
    dim.format = e.get_format();
    graph_.set_backbuffer_dimensions(dim);

    scene_loader_.get_scene().add_render_passes(graph_);

    AttachmentInfo back;

    auto &pass_graphics = graph_.add_pass("graphics", RENDER_GRAPH_QUEUE_GRAPHICS_BIT);
    auto &rt_back = pass_graphics.add_color_output("back", back);
    pass_graphics.set_get_clear_depth_stencil([](VkClearDepthStencilValue *value) -> bool {
      if (value) {
        value->depth = 1.0f;
        value->stencil = 0;
      }
      return true;
    });
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
      {
        auto &scene = scene_loader_.get_scene();
        context_.set_camera(cam_.get_projection(), cam_.get_view());
        VisibilityList visible;
        visible.clear();
        scene.gather_visible_opaque_renderables(context_.get_visibility_frustum(), visible);
        renderer_.set_mesh_renderer_options_from_lighting(lighting_);
        renderer_.set_mesh_renderer_options(renderer_.get_mesh_renderer_options());
        renderer_.begin();
        renderer_.push_renderables(context_, visible);
        Renderer::RendererOptionFlags opt = 0;
        renderer_.flush(cmd, context_, opt);
      }
      return;
      CommandBufferUtil::setup_fullscreen_quad(cmd, "builtin://shaders/quad.vert", "shaders://depth.frag");
      // CommandBufferUtil::setup_fullscreen_quad(
      //   cmd, "builtin://shaders/quad.vert",
      //   "assets://shaders/additive.frag");
      cmd.set_blend_enable(true);
      cmd.set_blend_factors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);

      auto &device_data = global_data_.device_data;
      cmd.set_storage_buffer(0, 0, *device_data.bvh_nodes_buffer);
      cmd.set_storage_buffer(0, 1, *device_data.bvh_vtx_buffer);
      cmd.set_storage_buffer(0, 2, *device_data.bvh_faces_buffer);
      cmd.set_storage_buffer(0, 4, *device_data.view_data_buffer);
      {
        DebugVars debug_vars = {}; // TODO
        cmd.push_constants(&debug_vars, 0, sizeof(debug_vars));
      }

      CommandBufferUtil::draw_fullscreen_quad(cmd, 80);
    });

    scene_loader_.get_scene().add_render_pass_dependencies(graph_, pass_graphics);

    graph_.set_backbuffer_source("back");
    graph_.bake();
    graph_.log();
  }

  void on_swapchain_destroyed(const SwapchainParameterEvent &) {}

  void render_frame(double frame_time, double elapsed_time) {
    auto &wsi = get_wsi();
    auto &device = wsi.get_device();
    auto &scene = scene_loader_.get_scene();

    // Per-frame updates
    // TODO: update ViewData
    scene.update_cached_transforms();

#if 0
    // Upload uniforms
    {
      ViewData view_data = {};
      const auto &cam = global_data_.scene_data.cam;
      view_data.view_uniforms.view_transform = cam.get_view();
      view_data.view_uniforms.eye_pos = cam.get_position();

      const float fovy_deg = 70.0f;
      const float fovy_rad = muglm::radians(fovy_deg);
      const float height = 2.0f * std::tanf(0.5f * fovy_rad);
      const auto &window_size = global_data_.swapchain_data.window_size;
      const float r_aspect = float(window_size.x) / float(window_size.y);
      view_data.view_uniforms.image_size_norm[0] = height * r_aspect;
      view_data.view_uniforms.image_size_norm[1] = height;
      const auto span = gsl::as_bytes(gsl::make_span(&view_data, 1));

      // GPUBufferTransfer::upload(span, view_uniforms.buffer,
      // VK_ACCESS_SHADER_READ_BIT);
    }
#endif

    scene.bind_render_graph_resources(graph_);
    graph_.setup_attachments(device, &device.get_swapchain_view());
    graph_.enqueue_render_passes(device);
  }

  RenderGraph graph_;
  GlobalData global_data_;

  FPSCamera cam_;
  LightingParameters lighting_;
  RenderContext context_;
  Renderer renderer_;
  SceneLoader scene_loader_;
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


  const std::string kFilename = "../assets/icosahedron.gltf";

  // Create the app object.

  std::unique_ptr<RenderGraphSandboxApplication> app;
  try {
    app = std::make_unique<RenderGraphSandboxApplication>(kFilename);
  } catch (const std::exception &e) {
    LOGE("application_create() threw exception: %s\n", e.what());
    return nullptr;
  }


  // Load geometry.
#if 0
  {
    LOGI("Loading geometry\n");

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;
    // const std::string filename = "../assets/lucy/lucy_watertight.obj";
    const bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, kFilename.c_str());

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

    std::unique_ptr<RadeonRays::Shape> rr_shape = std::make_unique<RadeonRays::Mesh>(
      vertices.data(), num_vertices, vertex_size_bytes, indices.data(), 0,
      face_vertex_counts.data(), num_faces);
    rr_shape->SetId(1);

    app->global_data_.scene_data.test_shape = std::move(rr_shape);
  }
#endif

  return app.release();
}
} // namespace Granite
