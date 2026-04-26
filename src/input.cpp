#include "internal/input.hpp"

namespace grf {

namespace {

bool inKeyRange(int key) {
  return key >= 0 && key < static_cast<int>(Input::Impl::kKeyBits);
}

bool inMouseRange(int button) {
  return button >= 0 && button < static_cast<int>(Input::Impl::kMouseBits);
}

}

Input::Impl::Impl(GLFWwindow * window) : m_window(window) {
  glfwSetWindowUserPointer(m_window, this);
  glfwSetKeyCallback(m_window,         &Impl::keyCallback);
  glfwSetMouseButtonCallback(m_window, &Impl::mouseButtonCallback);
  glfwSetCursorPosCallback(m_window,   &Impl::cursorPosCallback);
  glfwSetScrollCallback(m_window,      &Impl::scrollCallback);

  glfwGetCursorPos(m_window, &m_cursorX, &m_cursorY);
  m_cursorXPrev = m_cursorX;
  m_cursorYPrev = m_cursorY;
}

void Input::Impl::onPollBegin() {
  m_keyDownPrev   = m_keyDown;
  m_mouseDownPrev = m_mouseDown;
  m_cursorXPrev   = m_cursorX;
  m_cursorYPrev   = m_cursorY;
  m_scrollDX      = 0.0;
  m_scrollDY      = 0.0;
}

void Input::Impl::keyCallback(GLFWwindow * w, int key, int, int action, int) {
  auto * self = static_cast<Impl *>(glfwGetWindowUserPointer(w));
  if (!self || !inKeyRange(key)) return;

  if      (action == GLFW_PRESS)   self->m_keyDown.set(key, true);
  else if (action == GLFW_RELEASE) self->m_keyDown.set(key, false);
}

void Input::Impl::mouseButtonCallback(GLFWwindow * w, int button, int action, int) {
  auto * self = static_cast<Impl *>(glfwGetWindowUserPointer(w));
  if (!self || !inMouseRange(button)) return;

  if      (action == GLFW_PRESS)   self->m_mouseDown.set(button, true);
  else if (action == GLFW_RELEASE) self->m_mouseDown.set(button, false);
}

void Input::Impl::cursorPosCallback(GLFWwindow * w, double x, double y) {
  auto * self = static_cast<Impl *>(glfwGetWindowUserPointer(w));
  if (!self) return;
  self->m_cursorX = x;
  self->m_cursorY = y;
}

void Input::Impl::scrollCallback(GLFWwindow * w, double dx, double dy) {
  auto * self = static_cast<Impl *>(glfwGetWindowUserPointer(w));
  if (!self) return;
  self->m_scrollDX += dx;
  self->m_scrollDY += dy;
}

Input::Input(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
Input::~Input() = default;

bool Input::isPressed(Key k) const {
  int idx = static_cast<int>(k);
  if (!inKeyRange(idx)) return false;
  return m_impl->m_keyDown.test(idx);
}

bool Input::isJustPressed(Key k) const {
  int idx = static_cast<int>(k);
  if (!inKeyRange(idx)) return false;
  return m_impl->m_keyDown.test(idx) && !m_impl->m_keyDownPrev.test(idx);
}

bool Input::isJustReleased(Key k) const {
  int idx = static_cast<int>(k);
  if (!inKeyRange(idx)) return false;
  return !m_impl->m_keyDown.test(idx) && m_impl->m_keyDownPrev.test(idx);
}

bool Input::isPressed(MouseButton b) const {
  int idx = static_cast<int>(b);
  if (!inMouseRange(idx)) return false;
  return m_impl->m_mouseDown.test(idx);
}

bool Input::isJustPressed(MouseButton b) const {
  int idx = static_cast<int>(b);
  if (!inMouseRange(idx)) return false;
  return m_impl->m_mouseDown.test(idx) && !m_impl->m_mouseDownPrev.test(idx);
}

bool Input::isJustReleased(MouseButton b) const {
  int idx = static_cast<int>(b);
  if (!inMouseRange(idx)) return false;
  return !m_impl->m_mouseDown.test(idx) && m_impl->m_mouseDownPrev.test(idx);
}

std::pair<double, double> Input::cursorPos() const {
  return { m_impl->m_cursorX, m_impl->m_cursorY };
}

std::pair<double, double> Input::cursorDelta() const {
  return {
    m_impl->m_cursorX - m_impl->m_cursorXPrev,
    m_impl->m_cursorY - m_impl->m_cursorYPrev
  };
}

std::pair<double, double> Input::scrollDelta() const {
  return { m_impl->m_scrollDX, m_impl->m_scrollDY };
}

void Input::setCursorMode(CursorMode mode) {
  int glfwMode = GLFW_CURSOR_NORMAL;
  switch (mode) {
    case CursorMode::Normal:   glfwMode = GLFW_CURSOR_NORMAL;   break;
    case CursorMode::Hidden:   glfwMode = GLFW_CURSOR_HIDDEN;   break;
    case CursorMode::Captured: glfwMode = GLFW_CURSOR_DISABLED; break;
  }
  glfwSetInputMode(m_impl->m_window, GLFW_CURSOR, glfwMode);
}

}
