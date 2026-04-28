# Synchronization

Four primitives, each for a different scenario:

| Primitive               | Synchronizes               | Used for                              |
|-------------------------|----------------------------|---------------------------------------|
| `Fence` (binary)        | GPU → CPU                  | "Did this submission complete?"       |
| `Semaphore` (binary)    | GPU → GPU (cross-queue)    | Swapchain acquire / present interop   |
| Timeline semaphore      | GPU → GPU and GPU → CPU    | Internal: resource lifetime tracking  |
| Pipeline barrier        | GPU → GPU (intra-queue)    | Memory ordering within a cmd buffer   |

GRF exposes the first two as `Fence` and `Semaphore`. Timeline semaphores
are internal — used to track per-resource last-use values and the resource
manager's deferred uploads. Pipeline barriers are exposed as
`cmd.transition()`, `cmd.barrier()`, and `cmd.barrier(buf, ...)`.

────────────────────────────────────────────────────────────

## `Fence` — GPU → CPU

```cpp
auto fence = grf.createFence(/*signaled=*/false);

cmd.begin();
// record
cmd.end();

grf.submit(cmd, /*waits=*/{}, /*signals=*/{}, fence);
grf.waitFences({ fence });   // CPU blocks until GPU finishes
grf.resetFences({ fence });
```

Frame loop pattern: fence ring, one per frame in flight, `signaled=true`
on creation so the first frame's `waitFences` returns immediately.

```cpp
auto fences = grf.createFenceRing(/*signaled=*/true);

while (...) {
  auto [idx, dt] = grf.beginFrame();
  grf.waitFences({ fences[idx] });
  grf.resetFences({ fences[idx] });
  // record cmds[idx]
  grf.submit(cmds[idx], ..., fences[idx]);
}
```

────────────────────────────────────────────────────────────

## `Semaphore` — GPU → GPU

Binary semaphores. CPU never reads or writes them. Used for swapchain
interop:

```cpp
auto acquired = grf.createSemaphore();
auto rendered = grf.createSemaphore();

auto swap = grf.nextSwapchainImage(acquired);

cmd.begin();
cmd.transition(swap, Layout::Undefined, Layout::ColorAttachmentOptimal);
// draw to swap
cmd.transition(swap, Layout::ColorAttachmentOptimal, Layout::PresentSrc);
cmd.end();

grf.submit(cmd, /*waits=*/{ acquired }, /*signals=*/{ rendered }, fence);
grf.present(swap, /*waits=*/{ rendered });
```

`acquired` prevents the GPU from drawing before the swapchain image is
ready. `rendered` prevents the present from happening before the GPU
finishes drawing.

### Ring sizing

A binary semaphore signaled by a swapchain operation cannot be safely
reused until that operation fully retires. With 2 flight frames and 3
swapchain images, a 2-element ring would reuse semaphore[0] while it is
still tracked by the swapchain — `VUID-vkQueueSubmit-pSignalSemaphores-00067`.

`createSemaphoreRing()` sizes to `max(flightFrames, swapchainImageCount)`.

────────────────────────────────────────────────────────────

## Timeline values and resource lifetime

Internal timeline semaphores, one per queue (graphics, compute, transfer).
Every submission gets a timeline value; the GPU sets the timeline to that
value when the submission completes.

Each resource Impl stores `m_lastUseValues[3]` — the most recent timeline
value at which the resource was used, per queue. Command-recording methods
bump these.

When the last `shared_ptr` to a handle drops, the Impl's destructor schedules
a "grave" entry recording all three last-use values. `endFrame()` checks
which graves have all values retired (every queue caught up) and destroys
those.

You can drop a handle the moment you stop touching it on the CPU. The
framework keeps the GPU object alive until the GPU is done.

────────────────────────────────────────────────────────────

## Pipeline barriers

### `cmd.transition(img, fromLayout, toLayout)`

Move an image between layouts. Doubles as a memory barrier — the GPU
finishes whatever it was doing in the old layout before new-layout accesses
start. The framework picks correct stages and access masks from the layouts.

```cpp
cmd.transition(target, Layout::General, Layout::ShaderReadOptimal);
```

### `cmd.barrier()`

Global memory barrier. `eAllCommands → eAllCommands` with all access
flags. Pessimistic — drains the entire pipeline. Use when the dependency
does not involve image layouts and you do not want to think about exact
buffers.

```cpp
cmd.dispatch(...);
cmd.barrier();
cmd.draw(...);
```

### `cmd.barrier(buf, BufferAccess from, BufferAccess to)`

Buffer-scoped barrier with explicit access modes. Picks Vulkan stage and
access masks automatically.

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

## Queue ownership transfer

Required when a resource moves between queue families:

```cpp
// On graphics queue
cmd_g.transition(img, Layout::Undefined, Layout::General);
cmd_g.release(img, Layout::General, Layout::General, QueueType::Compute);
cmd_g.end();

// On compute queue
cmd_c.acquire(img, Layout::General, Layout::General, QueueType::Graphics);
// use img
cmd_c.end();
```

On hardware with a unified queue family (very common), the framework
collapses graphics/compute/transfer to the same family and these become
no-ops.

────────────────────────────────────────────────────────────

## Frame loop, annotated

```cpp
auto [idx, dt] = grf.beginFrame();
//  Polls events. Kicks pending uploads. Resets profiler slot.

grf.waitFences({ fences[idx] });
//  Wait for GPU's previous use of cmds[idx].
grf.resetFences({ fences[idx] });

auto swap = grf.nextSwapchainImage(acquired[idx]);
//  Acquire image. acquired[idx] signaled when ready.

cmds[idx].begin();
cmds[idx].transition(swap, Layout::Undefined, Layout::ColorAttachmentOptimal);
//  Layout transition + memory barrier.

cmds[idx].beginRendering({ ... });
cmds[idx].bindPipeline(pipe);
cmds[idx].draw(3);
cmds[idx].endRendering();

cmds[idx].transition(swap, Layout::ColorAttachmentOptimal, Layout::PresentSrc);
cmds[idx].end();

grf.submit(cmds[idx], { acquired[idx] }, { rendered[idx] }, fences[idx]);
//  GPU waits on acquired, signals rendered + fences[idx].

grf.present(swap, { rendered[idx] });
//  Present waits on rendered, queues image to compositor.

grf.endFrame();
//  Drain retired graves. Rotate frameIndex.
```
