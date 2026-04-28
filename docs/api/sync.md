# Sync — `Fence`, `Semaphore`

Both are PIMPL handles wrapping a `shared_ptr<Impl>`. Default-constructible
to a null state. Obtained from factories on `GRF`.

```cpp
class Fence {
public:
  Fence() = default;
};

class Semaphore {
public:
  Semaphore() = default;
};
```

The handles do not expose direct methods — synchronization is performed
through `GRF` (`waitFences`, `resetFences`, `submit`, `present`).

## `Fence`

GPU → CPU signal. Set by the GPU when a submission completes; readable on
the CPU.

```cpp
Fence       grf.createFence(bool signaled = false);
Ring<Fence> grf.createFenceRing(bool signaled = false);

grf.waitFences({ fence1, fence2 });    // CPU blocks until all signaled
grf.resetFences({ fence1, fence2 });   // back to unsignaled
grf.submit(cmd, ..., fence);           // signal on completion
```

`signaled = true` on creation: starts in the signaled state, so the first
`waitFences` returns immediately. Use this for fence rings where the first
frame would otherwise wait on a never-signaled fence.

## `Semaphore`

GPU → GPU signal. CPU never reads or writes them.

```cpp
Semaphore       grf.createSemaphore();
Ring<Semaphore> grf.createSemaphoreRing();
```

Used for swapchain interop:

```cpp
auto swap = grf.nextSwapchainImage(acquired);
grf.submit(cmd, /*waits=*/{ acquired }, /*signals=*/{ rendered }, fence);
grf.present(swap, /*waits=*/{ rendered });
```

`createSemaphoreRing` sizes to `max(flightFrames, swapchainImageCount)` —
larger than other rings to avoid swapchain semaphore reuse hazards
(`VUID-vkQueueSubmit-pSignalSemaphores-00067`). See
[concepts/synchronization.md](../concepts/synchronization.md#ring-sizing).

## Timeline semaphores (internal)

The framework also uses timeline semaphores internally — one per queue
(graphics, compute, transfer) — to track the per-resource last-use values
that drive deferred destruction. Not exposed in the public API.
