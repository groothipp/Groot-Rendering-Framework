# `Ring<T>`

Frame-rotated container. A `Ring<T>` holds `flightFrames` instances of `T`,
indexed by the current frame's rotation index.

```cpp
template <typename T>
class Ring {
public:
  Ring() = default;

  T&       operator[](uint32_t index);
  const T& operator[](uint32_t index) const;
};
```

Default-constructible to an empty ring. Indexing into an empty ring is
undefined behavior (the underlying `std::vector` is empty).

## Construction

Rings are created by the framework, not by user code. The available
factories on `GRF`:

```cpp
Ring<CommandBuffer> createCmdRing(QueueType);
Ring<Fence>         createFenceRing(bool signaled = false);
Ring<Semaphore>     createSemaphoreRing();
Ring<Buffer>        createBufferRing(BufferIntent, std::size_t bytes);
Ring<Img2D>         createImg2DRing(Format, uint32_t w, uint32_t h);
Ring<Img3D>         createImg3DRing(Format, uint32_t w, uint32_t h, uint32_t d);
```

Most rings are sized to `flightFrames` (default 2). Exception:
`createSemaphoreRing` sizes to `max(flightFrames, swapchainImageCount)` to
avoid swapchain semaphore reuse hazards (see
[concepts/synchronization.md](../concepts/synchronization.md)).

## Usage

```cpp
auto cmds   = grf.createCmdRing(grf::QueueType::Graphics);
auto fences = grf.createFenceRing(/*signaled=*/true);

while (grf.running()) {
  auto [idx, dt] = grf.beginFrame();

  grf.waitFences({ fences[idx] });
  grf.resetFences({ fences[idx] });

  cmds[idx].begin();
  // ...
  cmds[idx].end();

  grf.submit(cmds[idx], ..., fences[idx]);
}
```

## Storing in your own classes

Default-constructible, so this works:

```cpp
class MyRenderer {
  grf::Ring<grf::CommandBuffer> m_cmds;
  grf::Ring<grf::Semaphore>     m_acquired;
  grf::Ring<grf::Fence>         m_fences;

  void init(grf::GRF& grf) {
    m_cmds     = grf.createCmdRing(grf::QueueType::Graphics);
    m_acquired = grf.createSemaphoreRing();
    m_fences   = grf.createFenceRing(true);
  }
};
```

Implicit move-assignment from the factory does the right thing — the
internal `std::vector<T>` is moved over.
