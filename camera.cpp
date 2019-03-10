#include "camera.h"

// Modified version of cinder's CameraController class (https://github.com/cinder/Cinder).

void CameraController::tick(double time_delta)
{
  if (!mEnabled || !mCamera || !mWindow)
    return;

  constexpr float CAMERA_SPEED = 8.0f; // units per second
  const auto forward_vector = mCamera->getForwardVector();
  const auto right_vector = glm::rotate(mCamera->orientation, glm::vec3(1, 0, 0));
  const auto up_vector = glm::rotate(mCamera->orientation, glm::vec3(0, 1, 0));

  glm::vec3 pos_delta = glm::vec3(0);

  if (mMotionKeyState[0])
    pos_delta += forward_vector;
  if (mMotionKeyState[1])
    pos_delta -= forward_vector;
  if (mMotionKeyState[2])
    pos_delta -= right_vector;
  if (mMotionKeyState[3])
    pos_delta += right_vector;
  if (mMotionKeyState[4])
    pos_delta += up_vector;
  if (mMotionKeyState[5])
    pos_delta -= up_vector;

  pos_delta *= time_delta * CAMERA_SPEED;
  mCamera->eye_pos += pos_delta;
  mInitialCam.eye_pos += pos_delta;
}

void CameraController::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  const bool pressed = action != GLFW_RELEASE;
  if (key == GLFW_KEY_W) {
    mMotionKeyState[0] = pressed;
  } else if (key == GLFW_KEY_S) {
    mMotionKeyState[1] = pressed;
  } else if (key == GLFW_KEY_A) {
    mMotionKeyState[2] = pressed;
  } else if (key == GLFW_KEY_D) {
    mMotionKeyState[3] = pressed;
  } else if (key == GLFW_KEY_Q) {
    mMotionKeyState[4] = pressed;
  } else if (key == GLFW_KEY_E) {
    mMotionKeyState[5] = pressed;
  }
}

void CameraController::cursorPositionCallback(GLFWwindow *window, double xpos, double ypos)
{
  if (!mCamera || !mEnabled)
    return;

  const auto mousePos = glm::vec2(xpos, ypos);

  const bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  const bool middleDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
  const bool rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
  const bool altPressed = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS;
  const bool ctrlPressed = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;

  int action;
  if (rightDown || (leftDown && middleDown) || (leftDown && ctrlPressed))
    action = ACTION_ZOOM;
  else if (middleDown || (leftDown && altPressed))
    action = ACTION_PAN;
  else if (leftDown)
    action = ACTION_TUMBLE;
  else
    return;

  if (action != mLastAction) {
    mInitialCam = *mCamera;
    mInitialPivotDistance = mCamera->pivot_distance;
    mInitialMousePos = mousePos;
  }

  mLastAction = action;

  const auto initial_forward = mInitialCam.getForwardVector();
  const auto world_up = glm::vec3(0, 1, 0);
  const auto window_size = getWindowSize();

  if (action == ACTION_ZOOM) { // zooming
    auto mouseDelta = (mousePos.x - mInitialMousePos.x) + (mousePos.y - mInitialMousePos.y);

    float newPivotDistance =
      powf(2.71828183f, 2 * -mouseDelta / glm::length(glm::vec2(window_size))) * mInitialPivotDistance;
    glm::vec3 oldTarget = mInitialCam.eye_pos + initial_forward * mInitialPivotDistance;
    glm::vec3 newEye = oldTarget - initial_forward * newPivotDistance;
    mCamera->eye_pos = newEye;
    mCamera->pivot_distance = std::max<float>(newPivotDistance, mMinimumPivotDistance);

  } else if (action == ACTION_PAN) { // panning
    float deltaX = (mousePos.x - mInitialMousePos.x) / float(window_size.x) * mInitialPivotDistance;
    float deltaY = (mousePos.y - mInitialMousePos.y) / float(window_size.y) * mInitialPivotDistance;
    const auto right = glm::cross(initial_forward, world_up);
    mCamera->eye_pos = mInitialCam.eye_pos - right * deltaX + world_up * deltaY;

  } else { // tumbling
    float deltaX = (mousePos.x - mInitialMousePos.x) / -100.0f;
    float deltaY = (mousePos.y - mInitialMousePos.y) / 100.0f;
    glm::vec3 mW = normalize(initial_forward);

    glm::vec3 mU = normalize(cross(world_up, mW));

    const bool invertMotion = (mInitialCam.orientation * world_up).y < 0.0f;
    if (invertMotion) {
      deltaX = -deltaX;
      deltaY = -deltaY;
    }

    glm::vec3 rotatedVec = glm::angleAxis(deltaY, mU) * (-initial_forward * mInitialPivotDistance);
    rotatedVec = glm::angleAxis(deltaX, world_up) * rotatedVec;

    mCamera->eye_pos = mInitialCam.eye_pos + initial_forward * mInitialPivotDistance + rotatedVec;
    mCamera->orientation =
      glm::angleAxis(deltaX, world_up) * glm::angleAxis(deltaY, mU) * mInitialCam.orientation;
  }
}

void CameraController::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
  if (!mCamera || !mEnabled)
    return;

  if (action == GLFW_PRESS) {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    mInitialMousePos = glm::vec2(x, y);
    mInitialCam = *mCamera;
    mInitialPivotDistance = mCamera->pivot_distance;
    mLastAction = ACTION_NONE;

  } else if (action == GLFW_RELEASE) {
    mLastAction = ACTION_NONE;
  }
}

void CameraController::scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
  if (!mCamera || !mEnabled)
    return;

  // some mice issue mouseWheel events during middle-clicks; filter that out
  if (mLastAction != ACTION_NONE)
    return;

  const auto increment = float(yoffset);

  float multiplier;
  if (mMouseWheelMultiplier > 0)
    multiplier = powf(mMouseWheelMultiplier, increment);
  else
    multiplier = powf(-mMouseWheelMultiplier, -increment);
  const auto eye_dir = mCamera->getForwardVector();
  glm::vec3 newEye = mCamera->eye_pos + eye_dir * (mCamera->pivot_distance * (1 - multiplier));
  mCamera->eye_pos = newEye;
  mCamera->pivot_distance =
    std::max<float>(mCamera->pivot_distance * multiplier, mMinimumPivotDistance);
}
