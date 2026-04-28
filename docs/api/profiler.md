# `Profiler`

GPU-timestamp zone profiler with built-in ImGui readout. Owned by the
framework — access via `grf.profiler()`.

```cpp
class Profiler {
public:
  void render();
};
```

## Recording zones

Zones are recorded on `CommandBuffer`:

```cpp
cmd.beginProfile("frame");
  cmd.beginProfile("shadow pass");
    // ...
  cmd.endProfile();
  cmd.beginProfile("opaque");
    // ...
  cmd.endProfile();
cmd.endProfile();
```

Each `beginProfile` writes a timestamp via `vkCmdWriteTimestamp2` at
`eAllCommands` (after all preceding work completes); each `endProfile`
writes another. The duration is `(end_ticks - begin_ticks) * timestampPeriod`.

Limit is 64 zones per frame. Zones must nest (begin/end stack-based);
mismatched begin/end panics.

`beginProfile` / `endProfile` are no-ops if the framework was built without
the profiler subsystem (it always is in current builds; this is for
forward compatibility).

## ImGui readout

```cpp
grf.profiler().render();
```

Renders an ImGui window showing:

- FPS (current and rolling average)
- Frame time graph (last 256 frames, hover for per-frame value)
- Per-zone average durations in ms

Must be called between `gui().beginFrame()` and `gui().render(cmd)` like
any other ImGui call.

## Internal model

- Query pool sized to `kMaxZonesPerFrame * 2 * flightFrames` = 128 queries
  per slot.
- One slot per frame in flight.
- Slots rotate; results from slot N are read on the (N + flightFrames)th
  frame, with `eWait` to ensure GPU completion.
- `validBits` taken as the *minimum* across all queues with timestamp
  support, to mask off garbage high bits on hardware where queues differ
  (e.g., MoltenVK / Apple Silicon).
- Wrap-around handled via modular subtraction: `(end - begin) & mask`.
  Correct in both the normal and wrap cases.
- Rolling window: 120 samples per zone for the average.

## Limitations

- Only timestamp queries. No pipeline statistics, no occlusion queries.
- One profiler per `GRF` instance.
- Per-frame zone limit is a compile-time constant
  (`kMaxZonesPerFrame = 64`); exceeding it panics.
