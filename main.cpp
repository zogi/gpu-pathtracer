#include "common.h"
#include "camera.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMPLEMENTATION
#include <stb_image.h>

using RadeonRays::DeviceInfo;
using RadeonRays::IntersectionApi;

struct IntersectionApiDeleter {
  void operator()(IntersectionApi *p) { IntersectionApi::Delete(p); }
};
using IntersectionApiPtr = std::unique_ptr<IntersectionApi, IntersectionApiDeleter>;

struct GLFWwindowDeleter {
  void operator()(GLFWwindow *p) { glfwDestroyWindow(p); }
};
using GLFWwindowPtr = std::unique_ptr<GLFWwindow, GLFWwindowDeleter>;

class Window {
 public:
  Window();
  ~Window() = default;

  GLFWwindow *getGLFWwindow() const { return window_.get(); }

  bool shouldClose() const { return glfwWindowShouldClose(window_.get()); }

  void tick(double delta_time) { camera_controller_.tick(delta_time); }

 private:
  GLFWwindowPtr window_;
  Camera camera_;
  CameraController camera_controller_;
};

Window::Window()
{
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_DEPTH_BITS, 0);
  glfwWindowHint(GLFW_STENCIL_BITS, 0);
  glfwWindowHint(GLFW_MAXIMIZED, true);
  window_.reset(glfwCreateWindow(640, 480, "", nullptr, nullptr));
  if (!window_) {
    // g_logger->critical("Failed to create window.");
    abort();
  }
  glfwSetWindowUserPointer(window_.get(), this);

  // Init camera.
  camera_.eye_pos = glm::vec3(-1.6, 4.1, 4.4);
  camera_.orientation = glm::quatLookAt(glm::vec3(0.66, -0.48, -0.58), glm::vec3(0, 1, 0));
  camera_controller_.setWindow(window_.get());
  camera_controller_.setCamera(&camera_);
  camera_controller_.setEnabled(true);

  // Set up event callbacks.

  glfwSetFramebufferSizeCallback(window_.get(), [](GLFWwindow *, int width, int height) {
    // TODO: resize framebuffer
  });

  glfwSetKeyCallback(window_.get(), [](GLFWwindow *window_, int key, int scancode, int action, int mods) {
    // Imgui.
    ImGui_ImplGlfw_KeyCallback(window_, key, scancode, action, mods);
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
    ImGui_ImplGlfw_CharCallback(window_, codepoint);
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
    ImGui_ImplGlfw_MouseButtonCallback(window_, button, action, mods);
    const auto &io = ImGui::GetIO();
    if (io.WantCaptureMouse)
      return;
    // Camera.
    Window *window = (Window *)glfwGetWindowUserPointer(window_);
    window->camera_controller_.mouseButtonCallback(window_, button, action, mods);
  });

  glfwSetScrollCallback(window_.get(), [](GLFWwindow *window_, double xoffset, double yoffset) {
    // Imgui.
    ImGui_ImplGlfw_ScrollCallback(window_, xoffset, yoffset);
    const auto &io = ImGui::GetIO();
    if (io.WantCaptureMouse)
      return;
    // Camera.
    Window *window = (Window *)glfwGetWindowUserPointer(window_);
    window->camera_controller_.scrollCallback(window_, xoffset, yoffset);
  });
}

int main()
{
  struct GLFWGuard {
    GLFWGuard() { glfwInit(); }
    ~GLFWGuard() { glfwTerminate(); }
  } glfw_guard;


  // Init window.
  Window window;

  glfwMakeContextCurrent(window.getGLFWwindow());
  glfwSwapInterval(1); // Enable vsync

  gladLoadGL();

  struct ImGuiGuard {
    ImGuiGuard()
    {
      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      ImGui::StyleColorsClassic();
    }
    ~ImGuiGuard() { ImGui::DestroyContext(); }
  } imgui_guard;

  ImGui_ImplGlfw_InitForOpenGL(window.getGLFWwindow(), false);
  ImGui_ImplOpenGL3_Init("#version 450");

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
    IntersectionApi::SetPlatform(DeviceInfo::kOpenCL);
    return IntersectionApiPtr(IntersectionApi::Create(device_idx));
  }();

  // Use surface area heuristic for better intersection performance (but slower scene build time).
  intersection_api->SetOption("bvh.builder", "sah");

  // Load test geometry.
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
    attrib.vertices.data(), 3, 3 * sizeof(float), indices.data(), 0, numFaceVertices.data(),
    numFaceVertices.size());

  intersection_api->AttachShape(shape);
  intersection_api->Commit();

  while (!window.shouldClose()) {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    bool open = true;
    ImGui::ShowDemoWindow(&open);

    const auto &imgui_io = ImGui::GetIO();
    auto delta_time = imgui_io.DeltaTime;
    window.tick(delta_time);

    ImGui::Render();
    int display_w, display_h;
    glfwMakeContextCurrent(window.getGLFWwindow());
    glfwGetFramebufferSize(window.getGLFWwindow(), &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    const ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwMakeContextCurrent(window.getGLFWwindow());
    glfwSwapBuffers(window.getGLFWwindow());
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  return 0;
}
