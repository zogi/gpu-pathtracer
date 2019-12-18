#include "camera.h"

namespace {

constexpr float kScaleBase = 1.1f;

muglm::vec3
getEyePosAfterZoom(const muglm::vec3 &eye_pos, const muglm::vec3 &pivot, float delta_amount) {
  const float scale = powf(kScaleBase, delta_amount);
  return muglm::mix(pivot, eye_pos, scale);
}

} // unnamed namespace

// Modified version of cinder's CameraController class (https://github.com/cinder/Cinder).

void CameraController::tick(float time_delta) {
  if (!m_enabled || !m_camera) {
    return;
  }

  constexpr float CAMERA_SPEED = 8.0f; // units per second
  const auto forward_vector = m_camera->getForwardVector();
  const auto right_vector = m_camera->getRightVector();
  const auto up_vector = m_camera->getUpVector();

  muglm::vec3 pos_delta = muglm::vec3(0);

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

  pos_delta = (time_delta * CAMERA_SPEED) * pos_delta;
  m_camera->eye_pos += pos_delta;
  m_camera->pivot += pos_delta;
  m_initial_cam.eye_pos += pos_delta;
  m_initial_cam.pivot += pos_delta;
}

bool CameraController::on_key_pressed(const Granite::KeyboardEvent &e) {
  const bool pressed = e.get_key_state() == Granite::KeyState::Pressed;
  const auto key = e.get_key();
  if (key == Granite::Key::W) {
    m_motion_key_state[0] = pressed;
  } else if (key == Granite::Key::S) {
    m_motion_key_state[1] = pressed;
  } else if (key == Granite::Key::A) {
    m_motion_key_state[2] = pressed;
  } else if (key == Granite::Key::D) {
    m_motion_key_state[3] = pressed;
  } else if (key == Granite::Key::Q) {
    m_motion_key_state[4] = pressed;
  } else if (key == Granite::Key::E) {
    m_motion_key_state[5] = pressed;
  }
}

bool CameraController::on_mouse_move(const Granite::MouseMoveEvent &e) {
  // void CameraController::cursorPositionCallback(GLFWwindow *window, double xpos, double ypos) {
  if (!m_camera || !m_enabled) {
    return;
  }

  const float xpos = e.get_abs_x();
  const float ypos = e.get_abs_y();

  const auto mousePos = muglm::vec2(xpos, ypos);

  const bool leftDown = e.get_mouse_button_pressed(Granite::MouseButton::Left);
  const bool middleDown = e.get_mouse_button_pressed(Granite::MouseButton::Middle);
  const bool rightDown = e.get_mouse_button_pressed(Granite::MouseButton::Right);
  const bool altPressed = e.get_key_pressed(Granite::Key::LeftAlt);
  const bool ctrlPressed = e.get_key_pressed(Granite::Key::LeftCtrl);

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
  const auto world_up = muglm::vec3(0, 1, 0);
  const auto window_size = getWindowSize();
  const float initial_pivot_distance = m_initial_cam.getPivotDistance();

  if (action == ACTION_ZOOM) { // zooming
    const auto mouse_delta =
      (mousePos.x - m_initial_mouse_pos.x) + (mousePos.y - m_initial_mouse_pos.y);
    const auto delta_amount = -10 * mouse_delta / muglm::length(muglm::vec2(window_size));
    m_camera->eye_pos = getEyePosAfterZoom(m_initial_cam.eye_pos, m_initial_cam.pivot, delta_amount);

  } else if (action == ACTION_PAN) { // panning
    float deltaX = (mousePos.x - m_initial_mouse_pos.x) / float(window_size.x) * initial_pivot_distance;
    float deltaY = (mousePos.y - m_initial_mouse_pos.y) / float(window_size.y) * initial_pivot_distance;
    const auto right = muglm::cross(initial_forward, world_up);
    const muglm::vec3 delta = -right * deltaX + world_up * deltaY;
    m_camera->eye_pos = m_initial_cam.eye_pos + delta;
    m_camera->pivot = m_initial_cam.pivot + delta;

  } else { // tumbling
    float deltaX = (mousePos.x - m_initial_mouse_pos.x) / -100.0f;
    float deltaY = (mousePos.y - m_initial_mouse_pos.y) / 100.0f;
    muglm::vec3 mW = normalize(initial_forward);
    muglm::vec3 mU = normalize(cross(world_up, mW));

    const bool invertMotion = (m_initial_cam.orientation * world_up).y < 0.0f;
    if (invertMotion) {
      deltaX = -deltaX;
      deltaY = -deltaY;
    }

    const muglm::quat deltaRot = muglm::angleAxis(deltaX, world_up) * muglm::angleAxis(deltaY, mU);
    m_camera->eye_pos = m_initial_cam.pivot + deltaRot * (m_initial_cam.eye_pos - m_initial_cam.pivot);
    m_camera->orientation = deltaRot * m_initial_cam.orientation;
  }
}

bool CameraController::on_mouse_button(const Granite::MouseButtonEvent &e) {}

void CameraController::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
  if (!m_camera || !m_enabled)
    return;

  if (action == GLFW_PRESS) {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    m_initial_mouse_pos = muglm::vec2(x, y);
    m_initial_cam = *m_camera;
    m_last_action = ACTION_NONE;

  } else if (action == GLFW_RELEASE) {
    m_last_action = ACTION_NONE;
  }
}

void CameraController::scrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
  if (!m_camera || !m_enabled)
    return;

  // some mice issue mouseWheel events during middle-clicks; filter that out
  if (m_last_action != ACTION_NONE)
    return;

  m_camera->eye_pos = getEyePosAfterZoom(m_camera->eye_pos, m_camera->pivot, -float(yoffset));
}
