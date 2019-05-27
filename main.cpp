#include "camera.h"
#include "common.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMPLEMENTATION
#include <stb_image.h>

#include <shaders/depth.frag.spv.h>
#include <shaders/quad.vert.spv.h>
#include <spirv_reflect.h>

//#ifdef _WIN32
//#include <windows.h>
//#endif

constexpr const char *kPipelineCacheFileName = "pipeline_cache.bin";

// === Intersection ===

using RadeonRays::DeviceInfo;
using RadeonRays::IntersectionApi;

struct IntersectionApiDeleter {
  void operator()(IntersectionApi *p) { IntersectionApi::Delete(p); }
};
using IntersectionApiPtr = std::unique_ptr<IntersectionApi, IntersectionApiDeleter>;

// === Vulkan ===

//#define IMGUI_UNLIMITED_FRAME_RATE
#define IMGUI_VULKAN_DEBUG_REPORT

#define VKCHECK(vkresult)                       \
  do {                                          \
    const auto result = (vkresult);             \
    if (result < 0) {                           \
      fprintf(stderr, "VkResult %d\n", result); \
      abort();                                  \
    }                                           \
  } while (false)

static void checkVkResult(VkResult err) { VKCHECK(err); }

#ifdef IMGUI_VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(
  VkDebugReportFlagsEXT flags,
  VkDebugReportObjectTypeEXT objectType,
  uint64_t object,
  size_t location,
  int32_t messageCode,
  const char *pLayerPrefix,
  const char *pMessage,
  void *pUserData)
{
  (void)flags;
  (void)object;
  (void)location;
  (void)messageCode;
  (void)pUserData;
  (void)pLayerPrefix; // Unused arguments

  char message[4096];
  snprintf(message, ARRAYSIZE(message), "%d: %s\n", objectType, pMessage);
  fprintf(stderr, "%s", message);
#ifdef _WIN32
  // OutputDebugStringA(message);
#endif
  if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    assert(!"Validation error encountered!");
  return VK_FALSE;
}
#endif // IMGUI_VULKAN_DEBUG_REPORT

struct VulkanResources {
  VkAllocationCallbacks *allocator = NULL;
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  uint32_t queue_family = (uint32_t)-1;
  VkQueue queue = VK_NULL_HANDLE;
  VkDebugReportCallbackEXT debug_report = VK_NULL_HANDLE;
  VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
  VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
};

VulkanResources g_vulkan;

// === Window stuff ===

struct WindowResizeData {
  bool resize_wanted;
  int resize_width;
  int resize_height;
};

struct WindowData {
  ImGui_ImplVulkanH_WindowData imgui_data;
  WindowResizeData window_resize_data;
};

struct GLFWwindowDeleter {
  void operator()(GLFWwindow *p) { glfwDestroyWindow(p); }
};
using GLFWwindowPtr = std::unique_ptr<GLFWwindow, GLFWwindowDeleter>;

static void FramePresent(ImGui_ImplVulkanH_WindowData *wd);
static void FrameRender(ImGui_ImplVulkanH_WindowData *wd);

struct Extent {
  int width;
  int height;
};

class Window {
 public:
  Window(int width, int height);
  ~Window();

  GLFWwindow *getGLFWwindow() const { return window_.get(); }
  bool shouldClose() const { return glfwWindowShouldClose(window_.get()); }
  void tick(double delta_time);
  void setBackgroudColor(const ImVec4 &color);

  void render()
  {
    auto wd = &window_data_.imgui_data;
    FrameRender(wd);
    FramePresent(wd);
  }

  Extent extent() const
  {
    Extent res;
    glfwGetWindowSize(window_.get(), &res.width, &res.height);
    return res;
  }

  VkFormat colorFormat() const { return window_data_.imgui_data.SurfaceFormat.format; }

 private:
  GLFWwindowPtr window_;
  WindowData window_data_;
  Camera camera_;
  CameraController camera_controller_;
};

Window::Window(int width, int height)
{
  // Create GLFWwindow object.
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window_.reset(glfwCreateWindow(width, height, "", nullptr, nullptr));
  if (!window_) {
    abort();
  }
  glfwSetWindowUserPointer(window_.get(), this);

  // Create surface.
  VkSurfaceKHR surface;
  VKCHECK(glfwCreateWindowSurface(g_vulkan.instance, window_.get(), g_vulkan.allocator, &surface));

  // Create swapchain, framebuffer and other resources via ImGUI.
  {
    auto wd = &window_data_.imgui_data;
    wd->Surface = surface;

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_vulkan.physical_device, g_vulkan.queue_family, wd->Surface, &res);
    if (res != VK_TRUE) {
      fprintf(stderr, "Error no WSI support on physical device 0\n");
      exit(-1);
    }

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
                                                   VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
      g_vulkan.physical_device, wd->Surface, requestSurfaceImageFormat,
      (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

    // Select Present Mode
#ifdef IMGUI_UNLIMITED_FRAME_RATE
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR,
                                         VK_PRESENT_MODE_FIFO_KHR };
#else
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
      g_vulkan.physical_device, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
    // printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

    ImGui_ImplVulkanH_CreateWindowDataCommandBuffers(
      g_vulkan.physical_device, g_vulkan.device, g_vulkan.queue_family, wd, g_vulkan.allocator);
    ImGui_ImplVulkanH_CreateWindowDataSwapChainAndFramebuffer(
      g_vulkan.physical_device, g_vulkan.device, wd, g_vulkan.allocator, width, height);
  }

  // Init camera.
  camera_.eye_pos = glm::vec3(-1.6, 4.1, 4.4);
  camera_.orientation = glm::quatLookAt(glm::vec3(0.66, -0.48, -0.58), glm::vec3(0, 1, 0));
  camera_controller_.setWindow(window_.get());
  camera_controller_.setCamera(&camera_);
  camera_controller_.setEnabled(true);

  // Set up event callbacks.

  glfwSetFramebufferSizeCallback(window_.get(), [](GLFWwindow *window_, int width, int height) {
    Window *window = (Window *)glfwGetWindowUserPointer(window_);
    auto &resize_data = window->window_data_.window_resize_data;
    resize_data.resize_wanted = true;
    resize_data.resize_width = width;
    resize_data.resize_height = height;
  });

  glfwSetKeyCallback(window_.get(), [](GLFWwindow *window_, int key, int scancode, int action, int mods) {
    // Imgui.
    const auto &io = ImGui::GetIO();
    if (io.WantCaptureKeyboard)
      return;

    // Close glfw_window on ESC.
    if (key == GLFW_KEY_ESCAPE)
      glfwSetWindowShouldClose(window_, GLFW_TRUE);

    // Camera.
    Window *window = (Window *)glfwGetWindowUserPointer(window_);
    window->camera_controller_.keyCallback(window_, key, scancode, action, mods);
  });

  glfwSetCharCallback(window_.get(), [](GLFWwindow *window_, unsigned int codepoint) {
    // Imgui.
  });

  glfwSetCursorPosCallback(window_.get(), [](GLFWwindow *window_, double xpos, double ypos) {
    // Imgui.
    const auto &io = ImGui::GetIO();
    if (io.WantCaptureMouse)
      return;
    // Camera.
    Window *window = (Window *)glfwGetWindowUserPointer(window_);
    window->camera_controller_.cursorPositionCallback(window_, xpos, ypos);
  });

  glfwSetMouseButtonCallback(window_.get(), [](GLFWwindow *window_, int button, int action, int mods) {
    // Imgui.
    const auto &io = ImGui::GetIO();
    if (io.WantCaptureMouse)
      return;
    // Camera.
    Window *window = (Window *)glfwGetWindowUserPointer(window_);
    window->camera_controller_.mouseButtonCallback(window_, button, action, mods);
  });

  glfwSetScrollCallback(window_.get(), [](GLFWwindow *window_, double xoffset, double yoffset) {
    // Imgui.
    const auto &io = ImGui::GetIO();
    if (io.WantCaptureMouse)
      return;
    // Camera.
    Window *window = (Window *)glfwGetWindowUserPointer(window_);
    window->camera_controller_.scrollCallback(window_, xoffset, yoffset);
  });

  // Load pipeline cache.
  {
    std::vector<char> cache_data;

    if (std::ifstream cache_file(kPipelineCacheFileName); cache_file) {
      FILE *f = fopen(kPipelineCacheFileName, "r");
      // Determine file size
      fseek(f, 0, SEEK_END);
      const size_t size = ftell(f);
      cache_data.resize(size);
      rewind(f);
      fread(cache_data.data(), sizeof(char), size, f);
      fclose(f);
    }

    VkPipelineCacheCreateInfo create_info = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    create_info.initialDataSize = cache_data.size();
    create_info.pInitialData = cache_data.data();
    VKCHECK(vkCreatePipelineCache(g_vulkan.device, &create_info, nullptr, &g_vulkan.pipeline_cache));
  }

  // Init ImGui.
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  auto wd = &window_data_.imgui_data;

  ImGui_ImplGlfw_InitForVulkan(window_.get(), true);
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = g_vulkan.instance;
  init_info.PhysicalDevice = g_vulkan.physical_device;
  init_info.Device = g_vulkan.device;
  init_info.QueueFamily = g_vulkan.queue_family;
  init_info.Queue = g_vulkan.queue;
  init_info.PipelineCache = g_vulkan.pipeline_cache;
  init_info.DescriptorPool = g_vulkan.descriptor_pool;
  init_info.Allocator = g_vulkan.allocator;
  init_info.CheckVkResultFn = checkVkResult;
  ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);

  // Upload fonts.
  {
    // Use any command queue
    VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
    VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

    VkResult err = vkResetCommandPool(g_vulkan.device, command_pool, 0);
    VKCHECK(err);
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(command_buffer, &begin_info);
    VKCHECK(err);

    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &command_buffer;
    err = vkEndCommandBuffer(command_buffer);
    VKCHECK(err);
    err = vkQueueSubmit(g_vulkan.queue, 1, &end_info, VK_NULL_HANDLE);
    VKCHECK(err);

    err = vkDeviceWaitIdle(g_vulkan.device);
    VKCHECK(err);
    ImGui_ImplVulkan_InvalidateFontUploadObjects();
  }
}

Window::~Window()
{
  ImGui_ImplVulkanH_DestroyWindowData(
    g_vulkan.instance, g_vulkan.device, &window_data_.imgui_data, g_vulkan.allocator);

  VkResult err = vkDeviceWaitIdle(g_vulkan.device);
  VKCHECK(err);
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void Window::tick(double delta_time)
{
  // Handle resize.
  auto &resize_data = window_data_.window_resize_data;
  if (resize_data.resize_wanted) {
    ImGui_ImplVulkanH_CreateWindowDataSwapChainAndFramebuffer(
      g_vulkan.physical_device, g_vulkan.device, &window_data_.imgui_data, g_vulkan.allocator,
      resize_data.resize_width, resize_data.resize_height);
    resize_data.resize_wanted = false;
  }

  // Update camera controller.
  camera_controller_.tick(delta_time);
}

void Window::setBackgroudColor(const ImVec4 &color)
{
  memcpy(&window_data_.imgui_data.ClearValue.color.float32[0], &color, 4 * sizeof(float));
}

// === Main ===

static void glfwErrorCallback(int error, const char *description)
{
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

static void SetupVulkan(const char **extensions, uint32_t extensions_count);
static void CleanupVulkan();

// === SPIR-V reflection utilities ===

using SPIRVBytecodeView = gsl::span<uint8_t>;

std::vector<VkDescriptorSetLayout>
createDescriptorSetLayout(std::vector<SPIRVBytecodeView> spirv_bytecodes)
{
  // Load reflection data for the shaders.
  std::vector<SpvReflectShaderModule> shaders(spirv_bytecodes.size());
  for (int i = 0; i < shaders.size(); ++i) {
    auto &spirv = spirv_bytecodes[i];
    auto &shader = shaders[i];
    spvReflectCreateShaderModule(spirv.size(), spirv.data(), &shader);
  }

  // Collect bindings by set number and binding slot. Assert on collision.
  using SetNumber = uint32_t;
  using BindingNumber = uint32_t;
  struct BindingData {
    SpvReflectDescriptorBinding *p_binding;
    VkShaderStageFlags stage_flags;
  };
  std::unordered_map<SetNumber, std::map<BindingNumber, BindingData>> bindings;
  for (auto &shader : shaders) {
    for (size_t i = 0; i < shader.descriptor_binding_count; ++i) {
      auto &binding = shader.descriptor_bindings[i];
      auto [it, inserted] = bindings[binding.set].emplace(binding.binding, BindingData{ &binding, 0 });
      if (!inserted) {
        // This binding slot is already taken. Just assert if the types match. Linking should take
        // care of other mismatches.
        assert(binding.descriptor_type == it->second.descriptor_type);
      }
      it->second.stage_flags |= shader.shader_stage;
    }
  }

  // Create the descriptor set layouts.
  std::vector<VkDescriptorSetLayout> res;
  const size_t set_count = bindings.size();
  res.reserve(set_count);
  for (auto &[set_number, bindings_map] : bindings) {
    // Convert spirv reflection binding data to Vulkan binding data.
    std::vector<VkDescriptorSetLayoutBinding> vk_bindings;
    vk_bindings.reserve(bindings_map.size());
    for (auto &[binding_number, binding_data] : bindings_map) {
      auto p_binding = binding_data.p_binding;
      VkDescriptorSetLayoutBinding vk_binding = {};
      vk_binding.binding = binding_number;
      vk_binding.descriptorType = VkDescriptorType(p_binding->descriptor_type);
      vk_binding.descriptorCount = 1;
      for (uint32_t i = 0; i < p_binding->array.dims_count; ++i) {
        vk_binding.descriptorCount *= p_binding->array.dims[i];
      }
      vk_binding.stageFlags = binding_data.stage_flags;
      // TODO: immutable sampler support?
      // vk_binding.pImmutableSamplers;
      vk_bindings.push_back(vk_binding);
    }

    // Create descriptor set.
    VkDescriptorSetLayoutCreateInfo create_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    // TODO: push descriptors?
    // create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    create_info.bindingCount = uint32_t(vk_bindings.size());
    create_info.pBindings = vk_bindings.data();

    res.emplace_back();
    vkCreateDescriptorSetLayout(g_vulkan.device, &create_info, nullptr, &res.back());
  }

  // Free memory backing reflection data.
  for (auto &shader : shaders) {
    spvReflectDestroyShaderModule(&shader);
  }

  return res;
}

// The DescriptorSetLayoutData structure definition and the body of
// createDescriptorSetLayoutDataFromSPIRV were copied from
// external/SPIRV-Reflect/examples/main_descriptors.cpp
// The code has been modified to work as a function.
// License can be found at the end of external/SPIRV-Reflect/README.md

struct DescriptorSetLayoutData {
  uint32_t set_number;
  VkDescriptorSetLayoutCreateInfo create_info;
  std::vector<VkDescriptorSetLayoutBinding> bindings;
};

std::vector<DescriptorSetLayoutData> getDescriptorSetLayoutDataFromSPIRV(SPIRVBytecodeView spirv)
{
  SpvReflectShaderModule module = {};
  SpvReflectResult result = spvReflectCreateShaderModule(spirv.size_bytes(), spirv.data(), &module);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  uint32_t count = 0;
  result = spvReflectEnumerateDescriptorSets(&module, &count, NULL);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  std::vector<SpvReflectDescriptorSet *> sets(count);
  result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  std::vector<DescriptorSetLayoutData> set_layouts(sets.size(), DescriptorSetLayoutData{});
  for (size_t i_set = 0; i_set < sets.size(); ++i_set) {
    const SpvReflectDescriptorSet &refl_set = *(sets[i_set]);
    DescriptorSetLayoutData &layout = set_layouts[i_set];
    layout.bindings.resize(refl_set.binding_count);
    for (uint32_t i_binding = 0; i_binding < refl_set.binding_count; ++i_binding) {
      const SpvReflectDescriptorBinding &refl_binding = *(refl_set.bindings[i_binding]);
      VkDescriptorSetLayoutBinding &layout_binding = layout.bindings[i_binding];
      layout_binding.binding = refl_binding.binding;
      layout_binding.descriptorType = static_cast<VkDescriptorType>(refl_binding.descriptor_type);
      layout_binding.descriptorCount = 1;
      for (uint32_t i_dim = 0; i_dim < refl_binding.array.dims_count; ++i_dim) {
        layout_binding.descriptorCount *= refl_binding.array.dims[i_dim];
      }
      layout_binding.stageFlags = static_cast<VkShaderStageFlagBits>(module.shader_stage);
    }
    layout.set_number = refl_set.set;
    layout.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout.create_info.bindingCount = refl_set.binding_count;
    layout.create_info.pBindings = layout.bindings.data();
  }

  spvReflectDestroyShaderModule(&module);

  return set_layouts;
}

struct Program {
  std::vector<VkShaderModule> shaders;
  VkPipelineLayout pipeline_layout;
  std::vector<VkDescriptorSetLayout> desc_set_layouts;
};

void destroyProgram(const Program &program)
{
  for (const auto &shader : program.shaders) {
    vkDestroyShaderModule(g_vulkan.device, shader, nullptr);
  }
  vkDestroyPipelineLayout(g_vulkan.device, program.pipeline_layout, nullptr);
  for (const auto &desc_set_layout : program.desc_set_layouts) {
    vkDestroyDescriptorSetLayout(g_vulkan.device, desc_set_layout, nullptr);
  }
}

// === main ===

int main()
{
  struct GLFWGuard {
    GLFWGuard() { glfwInit(); }
    ~GLFWGuard() { glfwTerminate(); }
  } glfw_guard;

  glfwSetErrorCallback(glfwErrorCallback);

  // Setup Vulkan
  if (!glfwVulkanSupported()) {
    printf("GLFW: Vulkan Not Supported\n");
    return 1;
  }
  uint32_t extensions_count = 0;
  const char **extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
  SetupVulkan(extensions, extensions_count);

  // Init window.
  auto window = std::make_unique<Window>(1280, 720);
  window->setBackgroudColor(ImVec4(0.45f, 0.55f, 0.60f, 1.00f));

  // Init RadeonRays intersection API.
  IntersectionApiPtr intersection_api = []() {
    int device_idx = -1;
    for (auto idx = 0U; idx < IntersectionApi::GetDeviceCount(); ++idx) {
      DeviceInfo devinfo;
      IntersectionApi::GetDeviceInfo(idx, devinfo);

      if (devinfo.type == DeviceInfo::kGpu) {
        device_idx = idx;
      }
    }
    IntersectionApi::SetPlatform(DeviceInfo::kVulkan);
    return IntersectionApiPtr(IntersectionApi::Create(device_idx));
  }();

  // Use surface area heuristic for better intersection performance (but slower scene build time).
  intersection_api->SetOption("bvh.builder", "sah");

  // Load test geometry.
  {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;
    const std::string filename = "../models/lucy/lucy.obj";
    const bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str());

    const auto &mesh = shapes[0].mesh;

    std::vector<int> indices;
    indices.reserve(mesh.indices.size());
    for (const auto &tinyobj_index : mesh.indices) {
      indices.push_back(tinyobj_index.vertex_index);
    }

    std::vector<int> numFaceVertices(mesh.num_face_vertices.begin(), mesh.num_face_vertices.end());

    RadeonRays::Shape *shape = intersection_api->CreateMesh(
      attrib.vertices.data(), attrib.vertices.size() / 3, 3 * sizeof(float), indices.data(), 0,
      numFaceVertices.data(), numFaceVertices.size());

    intersection_api->AttachShape(shape);
    intersection_api->Commit();
  }

  // PSO - ray cast depth output
  Program ray_depth_program = {};
  VkPipeline ray_depth_pipeline = {};
  VkRenderPass render_pass = {};
  {
    VkAttachmentDescription color_attachment_desc = {};
    color_attachment_desc.format = window->colorFormat();
    color_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment_desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_attachment_desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_attachment_ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    {
      VkRenderPassCreateInfo create_info = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
      create_info.attachmentCount = 1;
      create_info.pAttachments = &color_attachment_desc;
      create_info.subpassCount = 1;
      create_info.pSubpasses = &subpass;
      create_info.dependencyCount = 0;
      create_info.pDependencies = nullptr;
      vkCreateRenderPass(g_vulkan.device, &create_info, nullptr, &render_pass);
    }

    // TODO: have to destroy the shaders modules, so the vulkan handles need to be stored somewhere.

    // Quad vertex shader.
    VkShaderModule quad_vs = {};
    {
      VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
      create_info.pCode = quad_vert_spirv;
      create_info.codeSize = sizeof(quad_vert_spirv);
      VKCHECK(vkCreateShaderModule(g_vulkan.device, &create_info, nullptr, &quad_vs));
    }

    // Depth fragment shader.
    VkShaderModule depth_fs = {};
    {
      VkShaderModuleCreateInfo createInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
      createInfo.pCode = depth_frag_spirv;
      createInfo.codeSize = sizeof(depth_frag_spirv);
      VKCHECK(vkCreateShaderModule(g_vulkan.device, &createInfo, nullptr, &depth_fs));
    }

    VkPipelineShaderStageCreateInfo stage_create_infos[2] = {};
    // Vertex shader.
    stage_create_infos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_infos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage_create_infos[0].module = quad_vs;
    stage_create_infos[0].pName = "main";
    // Fragment shader.
    stage_create_infos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_infos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage_create_infos[1].module = depth_fs;
    stage_create_infos[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vs_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
    };
    input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    input_assembly_state_create_info.primitiveRestartEnable = false;

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    // TODO: resize?
    const Extent window_extent = window->extent();
    viewport.width = float(window_extent.width);
    viewport.height = float(window_extent.height);

    VkRect2D scissor = {};
    scissor.extent.width = window_extent.width;
    scissor.extent.height = window_extent.height;

    // TODO: should be dynamic
    VkPipelineViewportStateCreateInfo viewport_state_create_info = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO
    };
    viewport_state_create_info.viewportCount = 1;
    viewport_state_create_info.pViewports = &viewport;
    viewport_state_create_info.scissorCount = 1;
    viewport_state_create_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo raster_state = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
    };
    raster_state.polygonMode = VK_POLYGON_MODE_FILL;
    raster_state.cullMode = VK_CULL_MODE_BACK_BIT;
    raster_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_state.depthBiasEnable = false;
    raster_state.lineWidth = 1.0;

    VkPipelineMultisampleStateCreateInfo multisample_state = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
    };
    multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_state.sampleShadingEnable = false;
    multisample_state.minSampleShading = 0.0;
    multisample_state.pSampleMask = nullptr;
    multisample_state.alphaToCoverageEnable = false;
    multisample_state.alphaToOneEnable = false;

    VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
    };
    depth_stencil_state.depthTestEnable = true;
    depth_stencil_state.depthWriteEnable = true;
    // Using inverse depth: 1 near, 0 far.
    depth_stencil_state.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
    depth_stencil_state.depthBoundsTestEnable = false;
    // depth_stencil_state.minDepthBounds;
    // depth_stencil_state.maxDepthBounds;
    depth_stencil_state.stencilTestEnable = false;
    // depth_stencil_state.front;
    // depth_stencil_state.back;

    VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
    color_blend_attachment_state.blendEnable = false;
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend_state = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO
    };
    color_blend_state.logicOpEnable = false;
    // color_blend_state.logicOp;
    color_blend_state.attachmentCount = 1; // Note: MUST equal to the subpass colorAttachmentCount
    color_blend_state.pAttachments = &color_blend_attachment_state;
    // color_blend_state.blendConstants[4];

    VkPipelineDynamicStateCreateInfo dynamic_state = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO
    };
    dynamic_state.dynamicStateCount = 0;
    dynamic_state.pDynamicStates = nullptr;

    // Get descriptor set layouts from the shader bytecodes.
    // TODO: have to destroy the set layouts, so the vulkan handles need to be stored somewhere.
    const auto quad_spirv = SPIRVBytecodeView((uint8_t *)quad_vert_spirv, sizeof(quad_vert_spirv));
    const auto depth_spirv = SPIRVBytecodeView((uint8_t *)depth_frag_spirv, sizeof(depth_frag_spirv));
    auto desc_set_layouts = createDescriptorSetLayout({ quad_spirv, depth_spirv });

    // Pipeline layout
    // TODO: have to destroy the pipeline layouts so the vulkan handle need to be stored somewhere.
    VkPipelineLayout pipeline_layout = {};
    {
      VkPipelineLayoutCreateInfo create_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
      create_info.setLayoutCount = uint32_t(desc_set_layouts.size());
      create_info.pSetLayouts = desc_set_layouts.data();
      create_info.pushConstantRangeCount = 0;
      create_info.pPushConstantRanges = nullptr;
      vkCreatePipelineLayout(g_vulkan.device, &create_info, nullptr, &pipeline_layout);
    }

    VkGraphicsPipelineCreateInfo create_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    create_info.stageCount = 2;
    create_info.pStages = stage_create_infos;
    create_info.pVertexInputState = &vs_create_info;
    create_info.pInputAssemblyState = &input_assembly_state_create_info;
    create_info.pViewportState = &viewport_state_create_info;
    create_info.pRasterizationState = &raster_state;
    create_info.pMultisampleState = &multisample_state;
    create_info.pDepthStencilState = &depth_stencil_state;
    create_info.pColorBlendState = &color_blend_state;
    create_info.pDynamicState = &dynamic_state;
    create_info.layout = pipeline_layout;
    create_info.renderPass = render_pass;
    create_info.subpass = 0;

    VKCHECK(vkCreateGraphicsPipelines(
      g_vulkan.device, g_vulkan.pipeline_cache, 1, &create_info, nullptr, &ray_depth_pipeline));

    ray_depth_program.shaders.push_back(quad_vs);
    ray_depth_program.shaders.push_back(depth_fs);
    ray_depth_program.pipeline_layout = pipeline_layout;
    ray_depth_program.desc_set_layouts = desc_set_layouts;
  }

  // Save pipeline cache data.
  {
    size_t data_size = 0;
    VKCHECK(vkGetPipelineCacheData(g_vulkan.device, g_vulkan.pipeline_cache, &data_size, nullptr));
    std::vector<char> cache(data_size);
    VKCHECK(vkGetPipelineCacheData(g_vulkan.device, g_vulkan.pipeline_cache, &data_size, cache.data()));
    std::ofstream cache_file(kPipelineCacheFileName);
    cache_file.write(cache.data(), cache.size());
    cache_file.close();
  }

  // Create command buffer pool and command buffer.
  VkCommandPool command_pool = {};
  VkCommandBuffer command_buffer = {};
  {
    VkCommandPoolCreateInfo create_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    // This means command buffers are short-lived. TODO: revisit later
    create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    create_info.queueFamilyIndex = g_vulkan.queue_family;
    VKCHECK(vkCreateCommandPool(g_vulkan.device, &create_info, nullptr, &command_pool));
  }
  {
    VkCommandBufferAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocate_info.commandPool = command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1;
    VKCHECK(vkAllocateCommandBuffers(g_vulkan.device, &allocate_info, &command_buffer));
  }

  while (!window->shouldClose()) {
    glfwPollEvents();

    const auto &imgui_io = ImGui::GetIO();
    auto delta_time = imgui_io.DeltaTime;
    window->tick(delta_time);

    // Frame start.
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // UI.
    bool open = true;
    ImGui::ShowDemoWindow(&open);

    // Render.
    ImGui::Render();
    window->render();

    VKCHECK(vkResetCommandPool(g_vulkan.device, command_pool, 0));
    {
      VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
      // TODO: revisit later
      begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      VKCHECK(vkBeginCommandBuffer(command_buffer, &begin_info));
    }

    // TODO

    VKCHECK(vkEndCommandBuffer(command_buffer));
  }

  intersection_api.reset();

  vkDestroyPipelineCache(g_vulkan.device, g_vulkan.pipeline_cache, nullptr);
  vkDestroyRenderPass(g_vulkan.device, render_pass, nullptr);
  destroyProgram(ray_depth_program);
  vkDestroyPipeline(g_vulkan.device, ray_depth_pipeline, nullptr);
  window.reset();
  CleanupVulkan();

  return 0;
}

static void SetupVulkan(const char **extensions, uint32_t extensions_count)
{
  VkResult err;

  // Create Vulkan Instance
  {
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.enabledExtensionCount = extensions_count;
    create_info.ppEnabledExtensionNames = extensions;

#ifdef IMGUI_VULKAN_DEBUG_REPORT
    const char *layers[] = { "VK_LAYER_KHRONOS_validation" };
    create_info.enabledLayerCount = 1;
    create_info.ppEnabledLayerNames = layers;

    // Enable debug report extension (we need additional storage, so we duplicate the user array to
    // add our new extension to it)
    const char **extensions_ext = (const char **)malloc(sizeof(const char *) * (extensions_count + 1));
    memcpy(extensions_ext, extensions, extensions_count * sizeof(const char *));
    extensions_ext[extensions_count] = "VK_EXT_debug_report";
    create_info.enabledExtensionCount = extensions_count + 1;
    create_info.ppEnabledExtensionNames = extensions_ext;

    // Create Vulkan Instance
    err = vkCreateInstance(&create_info, g_vulkan.allocator, &g_vulkan.instance);
    VKCHECK(err);
    free(extensions_ext);

    // Get the function pointer (required for any extensions)
    auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)
      vkGetInstanceProcAddr(g_vulkan.instance, "vkCreateDebugReportCallbackEXT");
    IM_ASSERT(vkCreateDebugReportCallbackEXT != NULL);

    // Setup the debug report callback
    VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
    debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
                            VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    debug_report_ci.pfnCallback = debug_report;
    debug_report_ci.pUserData = NULL;
    err = vkCreateDebugReportCallbackEXT(
      g_vulkan.instance, &debug_report_ci, g_vulkan.allocator, &g_vulkan.debug_report);
    VKCHECK(err);
#else
    // Create Vulkan Instance without any debug feature
    err = vkCreateInstance(&create_info, g_vulkan.allocator, &g_vulkan.instance);
    VKCHECK(err);
#endif
  }

  // Select GPU
  {
    uint32_t gpu_count;
    err = vkEnumeratePhysicalDevices(g_vulkan.instance, &gpu_count, NULL);
    VKCHECK(err);

    VkPhysicalDevice *gpus = (VkPhysicalDevice *)malloc(sizeof(VkPhysicalDevice) * gpu_count);
    err = vkEnumeratePhysicalDevices(g_vulkan.instance, &gpu_count, gpus);
    VKCHECK(err);

    // If a number >1 of GPUs got reported, you should find the best fit GPU for your purpose
    // e.g. VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU if available, or with the greatest memory available, etc.
    // for sake of simplicity we'll just take the first one, assuming it has a graphics queue family.
    g_vulkan.physical_device = gpus[0];
    free(gpus);
  }

  // Select graphics queue family
  {
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(g_vulkan.physical_device, &count, NULL);
    VkQueueFamilyProperties *queues =
      (VkQueueFamilyProperties *)malloc(sizeof(VkQueueFamilyProperties) * count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_vulkan.physical_device, &count, queues);
    for (uint32_t i = 0; i < count; i++)
      if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        g_vulkan.queue_family = i;
        break;
      }
    free(queues);
    IM_ASSERT(g_vulkan.queue_family != -1);
  }

  // Create Logical Device (with 1 queue)
  {
    int device_extension_count = 1;
    const char *device_extensions[] = { "VK_KHR_swapchain" };
    const float queue_priority[] = { 1.0f };
    VkDeviceQueueCreateInfo queue_info[1] = {};
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = g_vulkan.queue_family;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = queue_priority;
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
    create_info.pQueueCreateInfos = queue_info;
    create_info.enabledExtensionCount = device_extension_count;
    create_info.ppEnabledExtensionNames = device_extensions;
    err = vkCreateDevice(g_vulkan.physical_device, &create_info, g_vulkan.allocator, &g_vulkan.device);
    VKCHECK(err);
    vkGetDeviceQueue(g_vulkan.device, g_vulkan.queue_family, 0, &g_vulkan.queue);
  }

  // Create Descriptor Pool
  {
    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                                          { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                                          { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                                          { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                                          { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                                          { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                                          { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                                          { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                                          { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                                          { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                                          { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    err =
      vkCreateDescriptorPool(g_vulkan.device, &pool_info, g_vulkan.allocator, &g_vulkan.descriptor_pool);
    VKCHECK(err);
  }
}

static void CleanupVulkan()
{
  vkDestroyDescriptorPool(g_vulkan.device, g_vulkan.descriptor_pool, g_vulkan.allocator);

#ifdef IMGUI_VULKAN_DEBUG_REPORT
  // Remove the debug report callback
  auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)
    vkGetInstanceProcAddr(g_vulkan.instance, "vkDestroyDebugReportCallbackEXT");
  vkDestroyDebugReportCallbackEXT(g_vulkan.instance, g_vulkan.debug_report, g_vulkan.allocator);
#endif // IMGUI_VULKAN_DEBUG_REPORT

  vkDestroyDevice(g_vulkan.device, g_vulkan.allocator);
  vkDestroyInstance(g_vulkan.instance, g_vulkan.allocator);
}

static void FrameRender(ImGui_ImplVulkanH_WindowData *wd)
{
  VkResult err;

  VkSemaphore &image_acquired_semaphore = wd->Frames[wd->FrameIndex].ImageAcquiredSemaphore;
  err = vkAcquireNextImageKHR(
    g_vulkan.device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE,
    &wd->FrameIndex);
  VKCHECK(err);

  ImGui_ImplVulkanH_FrameData *fd = &wd->Frames[wd->FrameIndex];
  {
    err =
      vkWaitForFences(g_vulkan.device, 1, &fd->Fence, VK_TRUE, UINT64_MAX); // wait indefinitely instead
                                                                            // of periodically checking
    VKCHECK(err);

    err = vkResetFences(g_vulkan.device, 1, &fd->Fence);
    VKCHECK(err);
  }
  {
    err = vkResetCommandPool(g_vulkan.device, fd->CommandPool, 0);
    VKCHECK(err);
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
    VKCHECK(err);
  }
  {
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = wd->RenderPass;
    info.framebuffer = wd->Framebuffer[wd->FrameIndex];
    info.renderArea.extent.width = wd->Width;
    info.renderArea.extent.height = wd->Height;
    info.clearValueCount = 1;
    info.pClearValues = &wd->ClearValue;
    vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
  }

  // Record Imgui Draw Data and draw funcs into command buffer
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), fd->CommandBuffer);

  // Submit command buffer
  vkCmdEndRenderPass(fd->CommandBuffer);
  {
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &image_acquired_semaphore;
    info.pWaitDstStageMask = &wait_stage;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &fd->CommandBuffer;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = &fd->RenderCompleteSemaphore;

    err = vkEndCommandBuffer(fd->CommandBuffer);
    VKCHECK(err);
    err = vkQueueSubmit(g_vulkan.queue, 1, &info, fd->Fence);
    VKCHECK(err);
  }
}

static void FramePresent(ImGui_ImplVulkanH_WindowData *wd)
{
  ImGui_ImplVulkanH_FrameData *fd = &wd->Frames[wd->FrameIndex];
  VkPresentInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &fd->RenderCompleteSemaphore;
  info.swapchainCount = 1;
  info.pSwapchains = &wd->Swapchain;
  info.pImageIndices = &wd->FrameIndex;
  VkResult err = vkQueuePresentKHR(g_vulkan.queue, &info);
  VKCHECK(err);
}
