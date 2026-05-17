# Sync — `Sync`

GRF uses **timeline semaphores** as the single synchronization primitive for
GPU↔GPU and GPU↔CPU coordination. The public type users see is `Sync` — a
small value type that refers to a specific moment in time on one of the
framework's internal timelines.

```cpp
class Sync {
public:
  Sync() = default;
  bool valid() const;
};
```

`Sync` is cheap to copy (a handle + a value + a kind tag — no shared
ownership, no allocation). Default-constructed `Sync` is invalid; methods
that take a `Sync` skip invalid entries.

## Where `Sync`s come from

```cpp
Sync grf.submit(const CommandBuffer&, std::span<const Sync> waits = {});
```

Every submission returns a `Sync` representing "this work is complete." Pass
it to a later submit as a wait, to `present`, or `wait` on it from the CPU.

```cpp
Sync SwapchainImage::sync() const;
```

`nextSwapchainImage()` returns an image with an attached acquire `Sync`.
Pass it to the first submit that uses the image to gate rendering until the
swapchain image is actually ready.

## Operations on `Sync`

```cpp
void grf.wait(const Sync& s);
void grf.wait(std::span<const Sync> syncs);
```

CPU-side host wait. Blocks the calling thread until the GPU work the `Sync`
refers to has finished. Invalid `Sync`s are skipped. Panics if asked to
wait on a binary-semaphore `Sync` (only timeline `Sync`s support CPU wait —
in practice only swapchain `Sync`s are binary, and you never CPU-wait on
those).

```cpp
Ring<Sync> grf.createSyncRing();
```

Returns a `Ring<Sync>` with `flightFrames` default-constructed (invalid)
slots. The standard flight-frame pattern is to overwrite the slot with the
`Sync` from each frame's terminal submit, then `wait` on the slot at the
start of the next iteration.

## Frame-flight pattern

```cpp
Ring<CommandBuffer> cmdRing    = grf.createCmdRing(QueueType::Graphics);
Ring<Sync>          flightRing = grf.createSyncRing();

while (grf.running()) {
  auto [idx, dt] = grf.beginFrame();

  grf.wait(flightRing[idx]);            // host wait on prior use of cmdRing[idx]

  SwapchainImage swap = grf.nextSwapchainImage();

  cmdRing[idx].begin();
  /* ... record ... */
  cmdRing[idx].end();

  Sync done = grf.submit(cmdRing[idx], { swap.sync() });
  flightRing[idx] = done;               // record for next iteration
  grf.present(swap, { done });

  grf.endFrame();
}
```

Two rings replace the older four (cmd + 3× sync rings). The first frame's
`wait(flightRing[idx])` is a no-op because the slot starts default-invalid.

## Cross-queue pattern

```cpp
Sync computeDone  = grf.submit(computeCmd, {});
Sync graphicsDone = grf.submit(graphicsCmd, { computeDone, swap.sync() });
grf.present(swap, { graphicsDone });
```

`computeDone` flows into the graphics submit as a wait — the graphics work
won't start until compute finishes. No extra ring of cross-queue semaphores
needed.

## Internals

A `Sync` is a tagged union of two cases:

1. **Timeline sync** — refers to a value on one of the three internal
   per-queue timeline semaphores (graphics, compute, transfer). Used for all
   user-facing GPU↔GPU and GPU↔CPU sync.
2. **Binary sync** — refers to an internal swapchain acquire binary
   semaphore. Only produced by `SwapchainImage::sync()`; only consumed
   inside `submit` as a wait. `wait(s)` panics on a binary `Sync`.

The split exists because `vkAcquireNextImageKHR` and `vkQueuePresentKHR` in
core Vulkan only accept binary semaphores — that's the one place we can't
use timelines. Everywhere else (including `present`'s wait list) the
framework translates internally.
