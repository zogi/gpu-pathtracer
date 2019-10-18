#pragma once

#include "common.h"

struct Camera {
  glm::quat orientation;
  glm::vec3 eye_pos;

  glm::vec3 pivot = {};

  glm::vec3 getForwardVector() const { return glm::rotate(orientation, glm::vec3(0, 0, -1)); }
  glm::vec3 getUpVector() const { return glm::rotate(orientation, glm::vec3(0, 1, 0)); }
  glm::vec3 getRightVector() const { return glm::rotate(orientation, glm::vec3(1, 0, 0)); }

  float getPivotDistance() const { return glm::distance(eye_pos, pivot); }
};

// Modified version of cinder's CameraController class (https://github.com/cinder/Cinder).

class CameraController {
 public:
  CameraController(GLFWwindow *window = nullptr);
  void setWindow(GLFWwindow *window) { m_window = window; }
  void setCamera(Camera *camera_) { m_camera = camera_; }
  void setEnabled(bool enable) { m_enabled = enable; }

  void tick(double time_delta); // Called once each frame.

  void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
  void cursorPositionCallback(GLFWwindow *window, double xpos, double ypos);
  void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
  void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);

 private:
  enum { ACTION_NONE, ACTION_ZOOM, ACTION_PAN, ACTION_TUMBLE };

  glm::ivec2 getWindowSize() const;

  GLFWwindow *m_window;
  Camera *m_camera;
  bool m_enabled;

  glm::vec2 m_initial_mouse_pos;
  Camera m_initial_cam;
  int m_last_action;
  std::array<bool, 6> m_motion_key_state;
};
