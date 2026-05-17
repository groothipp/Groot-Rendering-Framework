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
Ring<Sync>          createSyncRing();
Ring<Buffer>        createBufferRing(BufferIntent, std::size_t bytes);
Ring<Img2D>         createImg2DRing(Format, uint32_t w, uint32_t h);
Ring<Img3D>         createImg3DRing(Format, uint32_t w, uint32_t h, uint32_t d);
```

All rings are sized to `flightFrames` (default 2).

## Usage

```cpp
auto cmds       = grf.createCmdRing(grf::QueueType::Graphics);
auto flightRing = grf.createSyncRing();

while (grf.running()) {
  auto [idx, dt] = grf.beginFrame();

  grf.wait(flightRing[idx]);

  cmds[idx].begin();
  // ...
  cmds[idx].end();

  grf::Sync done  = grf.submit(cmds[idx], { swap.sync() });
  flightRing[idx] = done;
}
```

`Ring<Sync>` slots start default-constructed (invalid). `grf.wait` on an
invalid `Sync` is a no-op, so the first frame doesn't hang waiting on
prior work that doesn't exist.

## Storing in your own classes

Default-constructible, so this works:

```cpp
class MyRenderer {
  grf::Ring<grf::CommandBuffer> m_cmds;
  grf::Ring<grf::Sync>          m_flight;

  void init(grf::GRF& grf) {
    m_cmds   = grf.createCmdRing(grf::QueueType::Graphics);
    m_flight = grf.createSyncRing();
  }
};
```

Implicit move-assignment from the factory does the right thing — the
internal `std::vector<T>` is moved over.
