#include "camera.h"

namespace {

constexpr float kScaleBase = 1.1f;

glm::vec3 getEyePosAfterZoom(const glm::vec3 &eye_pos, const glm::vec3 &pivot, float delta_amount)
{
  const float scale = powf(kScaleBase, delta_amount);
  return glm::lerp(pivot, eye_pos, scale);
}

} // unnamed namespace

// Modified version of cinder's CameraController class (https://github.com/cinder/Cinder).

CameraController::CameraController(GLFWwindow *window)
  : m_window(window)
  , m_camera(nullptr)
  , m_enabled(false)
  , m_initial_mouse_pos({})
  , m_last_action(ACTION_NONE)
{
  m_motion_key_state.fill(false);
}

glm::ivec2 CameraController::getWindowSize() const
{
  if (!m_window)
    return {};
  int w, h;
  glfwGetWindowSize(m_window, &w, &h);
  return { w, h };
}

void CameraController::tick(double time_delta)
{
  if (!m_enabled || !m_camera || !m_window)
    return;

  constexpr float CAMERA_SPEED = 8.0f; // units per second
  const auto forward_vector = m_camera->getForwardVector();
  const auto right_vector = m_camera->getRightVector();
  const auto up_vector = m_camera->getUpVector();

  glm::vec3 pos_delta = glm::vec3(0);

  if (m_motion_key_state[0])
    pos_delta += forward_vector;
  if (m_motion_key_state[1])
    pos_delta -= forward_vector;
  if (m_motion_key_state[2])
    pos_delta -= right_vector;
  if (m_motion_key_state[3])
    pos_delta += right_vector;
  if (m_motion_key_state[4])
    pos_delta += up_vector;
  if (m_motion_key_state[5])
    pos_delta -= up_vector;

  pos_delta *= time_delta * CAMERA_SPEED;
  m_camera->eye_pos += pos_delta;
  m_camera->pivot += pos_delta;
  m_initial_cam.eye_pos += pos_delta;
  m_initial_cam.pivot += pos_delta;
}

void CameraController::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
  const bool pressed = action != GLFW_RELEASE;
  if (key == GLFW_KEY_W) {
    m_motion_key_state[0] = pressed;
  } else if (key == GLFW_KEY_S) {
    m_motion_key_state[1] = pressed;
  } else if (key == GLFW_KEY_A) {
    m_motion_key_state[2] = pressed;
  } else if (key == GLFW_KEY_D) {
    m_motion_key_state[3] = pressed;
  } else if (key == GLFW_KEY_Q) {
    m_motion_key_state[4] = pressed;
  } else if (key == GLFW_KEY_E) {
    m_motion_key_state[5] = pressed;
  }
}

void CameraController::cursorPositionCallback(GLFWwindow *window, double xpos, double ypos)
{
  if (!m_camera || !m_enabled)
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

  if (action != m_last_action) {
    m_initial_cam = *m_camera;
    m_initial_mouse_pos = mousePos;
  }

  m_last_action = action;

  const auto initial_forward = m_initial_cam.getForwardVector();
  const auto world_up = glm::vec3(0, 1, 0);
  const auto window_size = getWindowSize();
  const float initial_pivot_distance = m_initial_cam.getPivotDistance();

  if (action == ACTION_ZOOM) { // zooming
    const auto mouse_delta =
      (mousePos.x - m_initial_mouse_pos.x) + (mousePos.y - m_initial_mouse_pos.y);
    const auto delta_amount = -10 * mouse_delta / glm::length(glm::vec2(window_size));
    m_camera->eye_pos = getEyePosAfterZoom(m_initial_cam.eye_pos, m_initial_cam.pivot, delta_amount);

  } else if (action == ACTION_PAN) { // panning
    float deltaX = (mousePos.x - m_initial_mouse_pos.x) / float(window_size.x) * initial_pivot_distance;
    float deltaY = (mousePos.y - m_initial_mouse_pos.y) / float(window_size.y) * initial_pivot_distance;
    const auto right = glm::cross(initial_forward, world_up);
    const glm::vec3 delta = -right * deltaX + world_up * deltaY;
    m_camera->eye_pos = m_initial_cam.eye_pos + delta;
    m_camera->pivot = m_initial_cam.pivot + delta;

  } else { // tumbling
    float deltaX = (mousePos.x - m_initial_mouse_pos.x) / -100.0f;
    float deltaY = (mousePos.y - m_initial_mouse_pos.y) / 100.0f;
    glm::vec3 mW = normalize(initial_forward);
    glm::vec3 mU = normalize(cross(world_up, mW));

    const bool invertMotion = (m_initial_cam.orientation * world_up).y < 0.0f;
    if (invertMotion) {
      deltaX = -deltaX;
      deltaY = -deltaY;
    }

    const glm::quat deltaRot = glm::angleAxis(deltaX, world_up) * glm::angleAxis(deltaY, mU);
    m_camera->eye_pos = m_initial_cam.pivot + deltaRot * (m_initial_cam.eye_pos - m_initial_cam.pivot);
    m_camera->orientation = deltaRot * m_initial_cam.orientation;
  }
}

void CameraController::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
  if (!m_camera || !m_enabled)
    return;

  if (action == GLFW_PRESS) {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    m_initial_mouse_pos = glm::vec2(x, y);
    m_initial_cam = *m_camera;
    m_last_action = ACTION_NONE;

  } else if (action == GLFW_RELEASE) {
    m_last_action = ACTION_NONE;
  }
}

void CameraController::scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
  if (!m_camera || !m_enabled)
    return;

  // some mice issue mouseWheel events during middle-clicks; filter that out
  if (m_last_action != ACTION_NONE)
    return;

  m_camera->eye_pos = getEyePosAfterZoom(m_camera->eye_pos, m_camera->pivot, -float(yoffset));
}
