# Synchronization

GRF uses **timeline semaphores** as the single sync primitive. One public
type, `Sync`, covers every GPU↔GPU and GPU↔CPU coordination scenario the
old `Fence` + binary `Semaphore` pair used to require.

| Concern                          | Mechanism                                |
|----------------------------------|------------------------------------------|
| GPU → CPU ("is this done yet?")  | `Sync` returned from `submit`, `wait(s)` |
| GPU → GPU same-queue ordering    | Pass a `Sync` into the next `submit`     |
| GPU → GPU cross-queue ordering   | Same: `Sync`s are queue-agnostic         |
| Swapchain acquire / present      | `SwapchainImage::sync()`, transparent    |
| Memory ordering within a cmd     | `cmd.transition`, `cmd.barrier`          |

There is no public `Fence`, no public binary `Semaphore`. Both collapsed
into `Sync`.

────────────────────────────────────────────────────────────

## Why one primitive

A binary semaphore answers "has this happened on the GPU?" but can't be
queried from the CPU. A fence answers "has this happened, period?" but can't
participate in a queue submit's wait list. Two types, two value spaces, two
sets of factory methods, two sets of ring patterns.

A timeline semaphore is a monotonic uint64 counter. The *same value* can be
queried via `vkWaitSemaphores` from the CPU and `pWaitSemaphoreValues` in a
queue submit. One primitive, one value space, one API.

────────────────────────────────────────────────────────────

## The `Sync` type

```cpp
class Sync {
public:
  Sync() = default;
  bool valid() const;
};
```

Cheap value type. Default-constructed `Sync` is invalid; operations skip
invalid entries.

Obtained from:
- `grf.submit(cmd, waits)` → `Sync` for "this work done"
- `SwapchainImage::sync()` → `Sync` for "image acquired"

Used by:
- `grf.submit(cmd, { sync, ... })` — GPU-side wait
- `grf.wait(sync)` — CPU-side wait
- `grf.present(swap, { sync })` — GPU-side wait before present
- `Ring<Sync>` slot — record per-frame for flight tracking

────────────────────────────────────────────────────────────

## Frame loop

```cpp
Ring<CommandBuffer> cmdRing    = grf.createCmdRing(QueueType::Graphics);
Ring<Sync>          flightRing = grf.createSyncRing();

while (grf.running()) {
  auto [idx, dt] = grf.beginFrame();

  grf.wait(flightRing[idx]);                       // host wait

  SwapchainImage swap = grf.nextSwapchainImage();

  cmdRing[idx].begin();
  cmdRing[idx].transition(swap, Layout::Undefined, Layout::ColorAttachmentOptimal);
  /* record draws */
  cmdRing[idx].transition(swap, Layout::ColorAttachmentOptimal, Layout::PresentSrc);
  cmdRing[idx].end();

  Sync done = grf.submit(cmdRing[idx], { swap.sync() });
  flightRing[idx] = done;
  grf.present(swap, { done });

  grf.endFrame();
}
```

Two rings instead of four. The flight `Sync` does double duty: it gates the
host wait *and* feeds present.

────────────────────────────────────────────────────────────

## Cross-queue

A `Sync` from a compute submit can wait on a graphics submit (or vice
versa). The framework's per-queue timeline semaphores all live in the same
value space.

```cpp
Sync computeDone  = grf.submit(computeCmd, {});
Sync graphicsDone = grf.submit(graphicsCmd, { computeDone, swap.sync() });
grf.present(swap, { graphicsDone });
```

No extra rings. No fences to thread between threads. The compute thread can
hand `computeDone` to the graphics thread by value — it's just `{ptr, u64}`.

────────────────────────────────────────────────────────────

## Resource upload sync (auto)

When you call `tex.write(data, ...)` or `buf.write(data, ...)`, the upload
runs on the framework's transfer queue. The next time you submit *any*
command buffer, the framework auto-injects a wait on the pending upload's
timeline value:

```cpp
tex.write(pixels, Layout::ShaderReadOptimal);  // queued upload

/* ... frame loop continues ... */

Sync done = grf.submit(cmd, { swap.sync() });
//          ^ framework also waits on pending upload internally
```

You never see this — `submit` is "just call submit." No manual
`waitForResourceUpdates` needed in your render loop. The host-side variant
remains available for tests and synchronous-init paths but is rarely needed.

────────────────────────────────────────────────────────────

## Pipeline barriers

Memory ordering *within* a command buffer is still pipeline barriers. These
live on `CommandBuffer`:

### `cmd.transition(img, fromLayout, toLayout)`

Move an image between layouts. Doubles as a memory barrier — the GPU
finishes whatever it was doing in the old layout before new-layout accesses
start. Stage and access masks inferred from the layouts.

```cpp
cmd.transition(target, Layout::General, Layout::ShaderReadOptimal);
```

`from` must match the image's currently-tracked layout (the framework
records this internally). `Undefined → X` is the only transition that does
not preserve contents.

### `cmd.barrier()`

Global memory barrier. `eAllCommands → eAllCommands` with all access
flags. Pessimistic — drains the entire pipeline.

```cpp
cmd.dispatch(...);
cmd.barrier();
cmd.draw(...);
```

### `cmd.barrier(buf, BufferAccess from, BufferAccess to)`

Buffer-scoped barrier with explicit access modes.

```cpp
cmd.dispatch(...);
cmd.barrier(positions, BufferAccess::ShaderWrite, BufferAccess::ShaderRead);
cmd.beginRendering(...);
cmd.draw(...);
cmd.endRendering();
```

| `BufferAccess`      | Stage                                       | Access                  |
|---------------------|---------------------------------------------|-------------------------|
| `ShaderRead`        | `eAllGraphics \| eComputeShader`            | `eShaderRead`           |
| `ShaderWrite`       | `eAllGraphics \| eComputeShader`            | `eShaderWrite`          |
| `TransferRead`      | `eTransfer`                                 | `eTransferRead`         |
| `TransferWrite`     | `eTransfer`                                 | `eTransferWrite`        |
| `IndirectRead`      | `eDrawIndirect`                             | `eIndirectCommandRead`  |
| `IndexRead`         | `eIndexInput`                               | `eIndexRead`            |

────────────────────────────────────────────────────────────

## Queue families

Images are created with `vk::SharingMode::eConcurrent` when the device
exposes distinct queue families for graphics, compute, and transfer.
Concurrent images may be accessed from any of the listed families without
ownership transfer barriers — no `cmd.acquire` / `cmd.release` needed at the
user level.

On hardware with a single queue family (Apple Silicon UMA, many integrated
GPUs), images stay `eExclusive`. The framework picks the right mode at
allocation time based on detected family indices.

The performance cost of concurrent sharing on dGPUs is typically 0–5% for
the workloads grf targets. If you later find a specific image where
exclusive sharing's compression wins are worth manual ownership transfers,
the underlying primitive supports it — but the public API doesn't expose it
right now.

────────────────────────────────────────────────────────────

## Resource lifetime

Every resource Impl carries `m_lastUseValues[3]` — the most recent timeline
value at which it was used on each of the three queues. Command-recording
methods bump these.

When the last `shared_ptr` to a handle drops, the Impl's destructor
schedules a "grave" entry recording all three last-use values. `endFrame()`
checks which graves have all values retired (every queue caught up) and
destroys those.

Drop a handle the moment you stop touching it on the CPU. The framework
keeps the GPU object alive until the GPU is done.

────────────────────────────────────────────────────────────

## Frame loop, annotated

```cpp
auto [idx, dt] = grf.beginFrame();
//  Polls events. Kicks pending uploads. Resets profiler slot.

grf.wait(flightRing[idx]);
//  Host wait for GPU's previous use of cmdRing[idx]. No-op on first frame
//  (default-constructed Sync is invalid).

auto swap = grf.nextSwapchainImage();
//  Acquire image. Internally signals a binary semaphore wrapped in
//  swap.sync().

cmdRing[idx].begin();
cmdRing[idx].transition(swap, Layout::Undefined, Layout::ColorAttachmentOptimal);
cmdRing[idx].beginRendering({ ... });
cmdRing[idx].bindPipeline(pipe);
cmdRing[idx].draw(3);
cmdRing[idx].endRendering();
cmdRing[idx].transition(swap, Layout::ColorAttachmentOptimal, Layout::PresentSrc);
cmdRing[idx].end();

Sync done = grf.submit(cmdRing[idx], { swap.sync() });
//  GPU waits on swap.sync() (the acquire), plus any pending resource
//  uploads. Returns a Sync representing when this work completes.

flightRing[idx] = done;
//  Record for next iteration's host wait.

grf.present(swap, { done });
//  Internally submits a tiny binary-sem signal that waits on `done` (a
//  timeline value), then calls vkQueuePresentKHR with the binary sem.

grf.endFrame();
//  Drain retired graves. Rotate frameIndex.
```
