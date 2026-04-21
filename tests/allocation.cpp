#include "public/gpu.hpp"
#include "public/image.hpp"
#include "public/structs.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE( "allocation: buffer", "[allocation][buffer]" ) {
  grf::GPU gpu;
  const std::size_t bufferSize = sizeof(uint32_t) * 1000;

  SECTION( "frequent update intent" ) {
    grf::Buffer buffer = gpu.createBuffer(grf::BufferIntent::FrequentUpdate, bufferSize);

    REQUIRE( buffer.valid() );
    CHECK( buffer.size() == bufferSize );
    CHECK( buffer.intent() == grf::BufferIntent::FrequentUpdate );
    CHECK( buffer.address() != 0x0 );
  }

  SECTION( "gpu only intent" ) {
    grf::Buffer buffer = gpu.createBuffer(grf::BufferIntent::GPUOnly, bufferSize);

    REQUIRE( buffer.valid() );
    CHECK( buffer.size() == bufferSize );
    CHECK( buffer.intent() == grf::BufferIntent::GPUOnly );
    CHECK( buffer.address() != 0x0 );
  }

  SECTION( "single update intent" ) {
    grf::Buffer buffer = gpu.createBuffer(grf::BufferIntent::SingleUpdate, bufferSize);

    REQUIRE( buffer.valid() );
    CHECK( buffer.size() == bufferSize );
    CHECK( buffer.intent() == grf::BufferIntent::SingleUpdate );
    CHECK( buffer.address() != 0x0 );
  }

  SECTION( "readable intent" ) {
    grf::Buffer buffer = gpu.createBuffer(grf::BufferIntent::Readable, bufferSize);

    REQUIRE( buffer.valid() );
    CHECK( buffer.size() == bufferSize );
    CHECK( buffer.intent() == grf::BufferIntent::Readable );
    CHECK( buffer.address() != 0x0 );
  }
}

TEST_CASE( "allocation: tex2D", "[allocation][tex2D]" ) {
  grf::GPU gpu;
  const uint32_t width = 1024;
  const uint32_t height = 1024;
  const grf::Format format = grf::Format::rgba16_sfloat;

  grf::Tex2D tex = gpu.createTex2D(format, width, height);

  REQUIRE(tex.valid() );

  auto [w, h, d] = tex.dims();
  CHECK( w == width );
  CHECK( h == height );
  CHECK( d == 1 );

  CHECK( tex.heapIndex() != 0xFFFFFFFF );
  CHECK( tex.size() != 0 );
  CHECK( tex.format() == format );
}

TEST_CASE( "allocation: tex3D", "[allocation][tex3D]" ) {
  grf::GPU gpu;
  const uint32_t width = 1024;
  const uint32_t height = 1024;
  const uint32_t depth = 3;
  const grf::Format format = grf::Format::rgba16_sfloat;

  grf::Tex3D tex = gpu.createTex3D(format, width, height, depth);

  REQUIRE( tex.valid() );

  auto [w, h, d] = tex.dims();
  CHECK( w == width );
  CHECK( h == height );
  CHECK( d == depth );

  CHECK( tex.heapIndex() != 0xFFFFFFFF );
  CHECK( tex.size() != 0 );
  CHECK( tex.format() == format );
}

TEST_CASE( "allocation: cubemap", "[allocation][cubemap]" ) {
  grf::GPU gpu;
  const uint32_t width = 1024;
  const uint32_t height = 1024;
  const grf::Format format = grf::Format::rgba16_sfloat;

  grf::Cubemap tex = gpu.createCubemap(format, width, height);

  REQUIRE( tex.valid() );

  auto [w, h, f] = tex.dims();
  CHECK( w == width );
  CHECK( h == height );
  CHECK( f == 6 );

  CHECK( tex.heapIndex() != 0xFFFFFFFF );
  CHECK( tex.size() != 0 );
  CHECK( tex.format() == format );
}

TEST_CASE( "allocation: img2D", "[allocation][img2D]" ) {
  grf::GPU gpu;
  const uint32_t width = 1024;
  const uint32_t height = 1024;
  const grf::Format format = grf::Format::rgba16_sfloat;

  grf::Img2D tex = gpu.createImg2D(format, width, height);

  REQUIRE(tex.valid() );

  auto [w, h, d] = tex.dims();
  CHECK( w == width );
  CHECK( h == height );
  CHECK( d == 1 );

  CHECK( tex.heapIndex() != 0xFFFFFFFF );
  CHECK( tex.size() != 0 );
  CHECK( tex.format() == format );
}

TEST_CASE( "allocation: img3D", "[allocation][img3D]" ) {
  grf::GPU gpu;
  const uint32_t width = 1024;
  const uint32_t height = 1024;
  const uint32_t depth = 3;
  const grf::Format format = grf::Format::rgba16_sfloat;

  grf::Img3D tex = gpu.createImg3D(format, width, height, depth);

  REQUIRE( tex.valid() );

  auto [w, h, d] = tex.dims();
  CHECK( w == width );
  CHECK( h == height );
  CHECK( d == depth );

  CHECK( tex.heapIndex() != 0xFFFFFFFF );
  CHECK( tex.size() != 0 );
  CHECK( tex.format() == format );
}

TEST_CASE( "allocation: sampler", "[allocation][sampler]" ) {
  grf::GPU gpu;
  const grf::SamplerSettings settings{
    .magFilter            = grf::Filter::Linear,
    .minFilter            = grf::Filter::Nearest,
    .uMode                = grf::SampleMode::ClampToBorder,
    .vMode                = grf::SampleMode::ClampToEdge,
    .wMode                = grf::SampleMode::MirroredRepeat,
    .anisotropicFiltering = true
  };

  grf::Sampler sampler = gpu.createSampler(settings);

  REQUIRE( sampler.valid() );
  CHECK(sampler.heapIndex() != 0xFFFFFFFF );
  CHECK( sampler.magFilter() == settings.magFilter );
  CHECK( sampler.minFilter() == settings.minFilter );
  CHECK( sampler.uMode() == settings.uMode );
  CHECK( sampler.vMode() == settings.vMode );
  CHECK( sampler.wMode() == settings.wMode );
  CHECK( sampler.anisotropicFiltering() == settings.anisotropicFiltering );
}