#pragma once

#include <memory>
#include <utility>

namespace grf {

enum class Key : int {
  Unknown        = -1,
  Space          = 32,
  Apostrophe     = 39,
  Comma          = 44,
  Minus          = 45,
  Period         = 46,
  Slash          = 47,
  Num0           = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
  Semicolon      = 59,
  Equal          = 61,
  A              = 65, B, C, D, E, F, G, H, I, J, K, L, M,
  N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
  LeftBracket    = 91,
  Backslash      = 92,
  RightBracket   = 93,
  GraveAccent    = 96,
  Escape         = 256,
  Enter          = 257,
  Tab            = 258,
  Backspace      = 259,
  Insert         = 260,
  Delete         = 261,
  Right          = 262,
  Left           = 263,
  Down           = 264,
  Up             = 265,
  PageUp         = 266,
  PageDown       = 267,
  Home           = 268,
  End            = 269,
  CapsLock       = 280,
  ScrollLock     = 281,
  NumLock        = 282,
  PrintScreen    = 283,
  Pause          = 284,
  F1             = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
  Numpad0        = 320, Numpad1, Numpad2, Numpad3, Numpad4,
  Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
  NumpadDecimal  = 330,
  NumpadDivide   = 331,
  NumpadMultiply = 332,
  NumpadSubtract = 333,
  NumpadAdd      = 334,
  NumpadEnter    = 335,
  NumpadEqual    = 336,
  LeftShift      = 340,
  LeftControl    = 341,
  LeftAlt        = 342,
  LeftSuper      = 343,
  RightShift     = 344,
  RightControl   = 345,
  RightAlt       = 346,
  RightSuper     = 347,
  Menu           = 348,
};

enum class MouseButton : int {
  Left    = 0,
  Right   = 1,
  Middle  = 2,
  Button4 = 3,
  Button5 = 4,
  Button6 = 5,
  Button7 = 6,
  Button8 = 7,
};

enum class CursorMode {
  Normal,
  Hidden,
  Captured,
};

class Input {
  friend class GRF;

public:
  class Impl;

  ~Input();
  Input(const Input&)            = delete;
  Input& operator=(const Input&) = delete;
  Input(Input&&)                 = delete;
  Input& operator=(Input&&)      = delete;

  bool isPressed(Key) const;
  bool isJustPressed(Key) const;
  bool isJustReleased(Key) const;

  bool isPressed(MouseButton) const;
  bool isJustPressed(MouseButton) const;
  bool isJustReleased(MouseButton) const;

  std::pair<double, double> cursorPos() const;
  std::pair<double, double> cursorDelta() const;
  std::pair<double, double> scrollDelta() const;

  void setCursorMode(CursorMode);

private:
  std::unique_ptr<Impl> m_impl;
  explicit Input(std::unique_ptr<Impl>);
};

}
