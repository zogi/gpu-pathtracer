#pragma once

#include "common.h"

struct Camera {
  glm::quat orientation;
  glm::vec3 eye_pos;
  float pivot_distance;

  Camera() : pivot_distance(0.01f) {}
  glm::vec3 getForwardVector() const { return glm::rotate(orientation, glm::vec3(0, 0, -1)); }
};

// Modified version of cinder's CameraController class (https://github.com/cinder/Cinder).

class CameraController {
 public:
  CameraController(GLFWwindow *window = nullptr)
    : mWindow(window)
    , mCamera(nullptr)
    , mInitialPivotDistance(0.01f)
    , mMouseWheelMultiplier(-1.1f)
    , mMinimumPivotDistance(0.01f)
    , mLastAction(ACTION_NONE)
    , mEnabled(false)
  {
  }
  void setWindow(GLFWwindow *window) { mWindow = window; }
  void setCamera(Camera *camera_) { mCamera = camera_; }
  void setEnabled(bool enable) { mEnabled = enable; }

  void tick(double time_delta); // Called once each frame.

  void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
  void cursorPositionCallback(GLFWwindow *window, double xpos, double ypos);
  void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);
  void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);

 private:
  enum { ACTION_NONE, ACTION_ZOOM, ACTION_PAN, ACTION_TUMBLE };

  glm::ivec2 getWindowSize() const
  {
    if (!mWindow)
      return {};
    int w, h;
    glfwGetWindowSize(mWindow, &w, &h);
    return { w, h };
  }

  glm::vec2 mInitialMousePos;
  Camera mInitialCam;
  Camera *mCamera;
  float mInitialPivotDistance;
  float mMouseWheelMultiplier, mMinimumPivotDistance;
  int mLastAction;
  std::array<bool, 6> mMotionKeyState;

  GLFWwindow *mWindow;
  bool mEnabled;
};
