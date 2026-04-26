#pragma once

#include "./pipelines.hpp"
#include "./resources.hpp"
#include "./types.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <variant>

namespace grf {

struct ImageBits;

struct ColorAttachment {
  std::variant<Tex2D, Img2D, SwapchainImage> img;
  LoadOp                loadOp     = LoadOp::Load;
  StoreOp               storeOp    = StoreOp::Store;
  std::array<float, 4>  clearValue = { 0.0f, 0.0f, 0.0f, 1.0f };
};

struct DepthAttachment {
  DepthImage  img;
  LoadOp      loadOp     = LoadOp::Clear;
  StoreOp     storeOp    = StoreOp::Store;
  float       clearValue = 1.0f;
};

using TransitionImage = std::variant<
  Tex2D, Tex3D, Cubemap, Img2D, Img3D, DepthImage, SwapchainImage
>;

class CommandBuffer {
  friend class GRF;
  friend class GUI;

  class Impl;
  std::shared_ptr<Impl> m_impl;

public:
  void begin();
  void end();

  void bindPipeline(const ComputePipeline&);
  void bindPipeline(const GraphicsPipeline&);
  void bindIndexBuffer(const Buffer&, IndexFormat = IndexFormat::U32, std::size_t offset = 0);

  void pushBytes(std::span<const std::byte> data, uint32_t offset = 0);

  template <typename T>
  void push(const T& data, uint32_t offset = 0) {
    static_assert(std::is_trivially_copyable_v<T>);
    pushBytes(std::as_bytes(std::span{ &data, 1 }), offset);
  }

  void beginRendering(
    std::span<const ColorAttachment> colors,
    const std::optional<DepthAttachment>& depth = std::nullopt
  );
  void endRendering();

  void draw(uint32_t vertexCount, uint32_t instanceCount = 1,
            uint32_t firstVertex = 0, uint32_t firstInstance = 0);
  void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                   uint32_t firstIndex = 0, int32_t vertexOffset = 0,
                   uint32_t firstInstance = 0);
  void drawIndirect(const Buffer&, std::size_t offset, uint32_t drawCount, uint32_t stride);
  void drawIndexedIndirect(const Buffer&, std::size_t offset, uint32_t drawCount, uint32_t stride);

  void dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1);
  void dispatchIndirect(const Buffer&, std::size_t offset = 0);

  void transition(const TransitionImage& img, Layout from, Layout to);
  void release(const TransitionImage& img, Layout from, Layout to, QueueType dstQueue);
  void acquire(const TransitionImage& img, Layout from, Layout to, QueueType srcQueue);

private:
  explicit CommandBuffer(std::shared_ptr<Impl>);

  static ImageBits extractColor(const ColorAttachment&);
  static ImageBits extractDepth(const DepthAttachment&);
  static ImageBits extractAny(const TransitionImage&);
};

}
