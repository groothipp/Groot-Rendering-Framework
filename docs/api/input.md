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
std::pair<double, double> cursorPos()    const;   // normalized to [-1, 1]
std::pair<double, double> cursorDelta()  const;   // since last frame, same scale
std::pair<double, double> scrollDelta()  const;   // raw scroll units, since last frame
```

Cursor position is normalized to the window: `(-1, -1)` is the top-left
corner, `(+1, +1)` is the bottom-right, `(0, 0)` is the center. Y is down,
matching GLFW / Vulkan convention — flip the sign if you want Y-up for
FPS-style camera math.

`cursorDelta` uses the same scale (full-screen swipe = `±2` along that axis),
not pixel units. Bounded by `[-2, 2]` in the pathological teleport case but
typically much smaller per frame; multiply by a sensitivity factor before
applying as camera rotation.

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
