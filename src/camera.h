#pragma once

#include <array>

#include <granite/application/application_wsi_events.hpp>
#include <granite/application/input/input.hpp>
#include <granite/event/event.hpp>
#include <granite/math/muglm/muglm.hpp>
#include <granite/math/muglm/muglm_impl.hpp>

struct Camera {
  muglm::quat orientation;
  muglm::vec3 eye_pos;

  muglm::vec3 pivot = {};

  muglm::ivec2 viewport_size = {};

  muglm::vec3 getForwardVector() const { return orientation * muglm::vec3(0, 0, -1); }
  muglm::vec3 getUpVector() const { return orientation * muglm::vec3(0, 1, 0); }
  muglm::vec3 getRightVector() const { return orientation * muglm::vec3(1, 0, 0); }

  float getPivotDistance() const { return muglm::distance(eye_pos, pivot); }
};

// Modified version of cinder's CameraController class (https://github.com/cinder/Cinder).

class CameraController : public Granite::EventHandler {
 public:
  CameraController() {
    m_motion_key_state.fill(false);
    EVENT_MANAGER_REGISTER(CameraController, on_key_pressed, Granite::KeyboardEvent);
    EVENT_MANAGER_REGISTER(CameraController, on_mouse_move, Granite::MouseMoveEvent);
    EVENT_MANAGER_REGISTER(CameraController, on_mouse_button, Granite::MouseButtonEvent);

    EVENT_MANAGER_REGISTER_LATCH(
      CameraController, on_swapchain_created, on_swapchain_destroyed, Vulkan::SwapchainParameterEvent);
  }

  void setCamera(Camera *camera_) { m_camera = camera_; }
  void setEnabled(bool enable) { m_enabled = enable; }

  void tick(float time_delta); // Called once each frame.

  bool on_key_pressed(const Granite::KeyboardEvent &e);
  bool on_mouse_move(const Granite::MouseMoveEvent &e);
  bool on_mouse_button(const Granite::MouseButtonEvent &e);
  // TODO: scroll callback
  // void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);

  void on_swapchain_created(const Vulkan::SwapchainParameterEvent &e);
  void on_swapchain_destroyed(const Vulkan::SwapchainParameterEvent &) {}

 private:
  enum { ACTION_NONE, ACTION_ZOOM, ACTION_PAN, ACTION_TUMBLE };

  Camera *m_camera = nullptr;
  bool m_enabled = false;

  muglm::vec2 m_initial_mouse_pos = {};
  Camera m_initial_cam = {};
  int m_last_action = ACTION_NONE;
  std::array<bool, 6> m_motion_key_state;
};
