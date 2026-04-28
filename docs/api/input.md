# `Input`

Keyboard, mouse, and cursor state. Owned by the framework; access via
`grf.input()`. Not copyable, not movable.

State is updated by `grf.beginFrame()` (which polls window events and
processes the input deltas).

## Keyboard

```cpp
bool isPressed(Key)        const;   // currently held
bool isJustPressed(Key)    const;   // pressed THIS frame, not last
bool isJustReleased(Key)   const;   // released THIS frame, not last
```

`Key` enum values match GLFW's key codes
(`Key::Space = 32`, `Key::A = 65`, `Key::F1 = 290`, etc.). See
[`<grf/input.hpp>`](../../public/input.hpp) for the full list.

## Mouse buttons

```cpp
bool isPressed(MouseButton)        const;
bool isJustPressed(MouseButton)    const;
bool isJustReleased(MouseButton)   const;
```

```cpp
enum class MouseButton {
  Left, Right, Middle,
  Button4, Button5, Button6, Button7, Button8,
};
```

## Cursor and scroll

```cpp
std::pair<double, double> cursorPos()    const;   // pixels from top-left
std::pair<double, double> cursorDelta()  const;   // since last frame
std::pair<double, double> scrollDelta()  const;   // since last frame
```

Deltas reset to zero at the start of each frame's poll.

## Cursor mode

```cpp
void setCursorMode(CursorMode);

enum class CursorMode { Normal, Hidden, Captured };
```

- `Normal` — visible, free movement.
- `Hidden` — invisible while inside the window, free movement.
- `Captured` — invisible, locked to the window center, infinite movement.
  Use for FPS-style cameras.

## ImGui interop

Combine with `grf.gui().wantsMouse()` / `wantsKeyboard()` to gate input
when ImGui is consuming events:

```cpp
if (!grf.gui().wantsKeyboard()) {
  if (grf.input().isPressed(grf::Key::W)) /* move forward */;
}

if (!grf.gui().wantsMouse()) {
  auto [dx, dy] = grf.input().cursorDelta();
  /* rotate camera */;
}
```
