#pragma once

#include "public/input.hpp"

#include <GLFW/glfw3.h>

#include <bitset>

namespace grf {

class Input::Impl {
public:
  static constexpr std::size_t kKeyBits   = 512;  // covers GLFW_KEY_LAST (348)
  static constexpr std::size_t kMouseBits = 8;    // GLFW_MOUSE_BUTTON_8 + 1

  GLFWwindow * m_window = nullptr;

  std::bitset<kKeyBits>   m_keyDown;
  std::bitset<kKeyBits>   m_keyDownPrev;
  std::bitset<kMouseBits> m_mouseDown;
  std::bitset<kMouseBits> m_mouseDownPrev;

  double m_cursorX        = 0.0;
  double m_cursorY        = 0.0;
  double m_cursorXPrev    = 0.0;
  double m_cursorYPrev    = 0.0;
  double m_scrollDX       = 0.0;
  double m_scrollDY       = 0.0;

  explicit Impl(GLFWwindow * window);

  void onPollBegin();

private:
  static void keyCallback(GLFWwindow *, int key, int scancode, int action, int mods);
  static void mouseButtonCallback(GLFWwindow *, int button, int action, int mods);
  static void cursorPosCallback(GLFWwindow *, double x, double y);
  static void scrollCallback(GLFWwindow *, double dx, double dy);
};

}
