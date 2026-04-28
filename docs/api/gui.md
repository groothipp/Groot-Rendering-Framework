# `GUI`

ImGui integration. Owned by the framework — access via `grf.gui()`. Not
copyable, not movable.

```cpp
class GUI {
public:
  void beginFrame();
  void render(CommandBuffer&);

  bool wantsMouse()    const;
  bool wantsKeyboard() const;
};
```

## Lifecycle

ImGui's `NewFrame` / `Render` lifecycle is opt-in per frame. The framework
does not auto-`beginFrame` on `GRF::beginFrame`; if you want UI you call
`gui().beginFrame()` explicitly:

```cpp
auto [idx, dt] = grf.beginFrame();
grf.gui().beginFrame();          // optional — only if building UI this frame

ImGui::Begin("Stats");
// ImGui calls here
ImGui::End();

// later, inside a beginRendering / endRendering pass:
grf.gui().render(cmd);
```

`render(cmd)` calls `ImGui::Render()`, gets the draw data, and submits it
via `ImGui_ImplVulkan_RenderDrawData` against the bound color attachment.
Must be called inside an active `beginRendering / endRendering` pass.

## Skipped frames

If `beginFrame()` was never called this frame, `render(cmd)` is a no-op.
If `beginFrame()` was called but `render(cmd)` was not (e.g., a conditional
path skipped UI this frame), the next `beginFrame()` calls
`ImGui::EndFrame()` to balance the dangling state — no validation panic,
no stale frame.

## Input capture

```cpp
bool wantsMouse()    const;
bool wantsKeyboard() const;
```

Forwarders to `ImGui::GetIO().WantCaptureMouse` /
`WantCaptureKeyboard`. Use these to gate your own input handling — when
ImGui wants the input, your camera / picking logic should ignore it.

Updated by `gui().beginFrame()`. If you do not call `beginFrame()`, these
return stale values from the last frame that did call it.

## Direct ImGui access

The framework's install ships `<grf/imgui.h>` and `<grf/imconfig.h>`. You
can call ImGui directly from your application code — it shares the
context the framework set up.

The framework auto-creates a docking-enabled main viewport and applies a
green-brown style at startup. You can override the style after the
framework constructs:

```cpp
grf::GRF grf;
ImGui::StyleColorsDark();   // or whatever you prefer
```

## Pool sizing

ImGui owns its own descriptor pool internally
(`init_info.DescriptorPoolSize = 1024`). This is the maximum number of
texture slots ImGui can register via `ImGui_ImplVulkan_AddTexture()`. For
typical UIs this is more than sufficient.
