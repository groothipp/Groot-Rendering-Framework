#include "public/grf.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE( "allocation: buffer", "[allocation][buffer]" ) {
  grf::GRF grf;
  const std::size_t bufferSize = sizeof(uint32_t) * 1000;

  SECTION( "frequent update intent" ) {
    grf::Buffer buffer = grf.createBuffer(grf::BufferIntent::FrequentUpdate, bufferSize);

    CHECK( buffer.size() == bufferSize );
    CHECK( buffer.intent() == grf::BufferIntent::FrequentUpdate );
    CHECK( buffer.address() != 0x0 );
  }

  SECTION( "gpu only intent" ) {
    grf::Buffer buffer = grf.createBuffer(grf::BufferIntent::GPUOnly, bufferSize);

    CHECK( buffer.size() == bufferSize );
    CHECK( buffer.intent() == grf::BufferIntent::GPUOnly );
    CHECK( buffer.address() != 0x0 );
  }

  SECTION( "single update intent" ) {
    grf::Buffer buffer = grf.createBuffer(grf::BufferIntent::SingleUpdate, bufferSize);

    CHECK( buffer.size() == bufferSize );
    CHECK( buffer.intent() == grf::BufferIntent::SingleUpdate );
    CHECK( buffer.address() != 0x0 );
  }

  SECTION( "readable intent" ) {
    grf::Buffer buffer = grf.createBuffer(grf::BufferIntent::Readable, bufferSize);

    CHECK( buffer.size() == bufferSize );
    CHECK( buffer.intent() == grf::BufferIntent::Readable );
    CHECK( buffer.address() != 0x0 );
  }
}

TEST_CASE( "allocation: tex2D", "[allocation][tex2D]" ) {
  grf::GRF grf;
  const uint32_t width = 1024;
  const uint32_t height = 1024;
  const grf::Format format = grf::Format::rgba16_sfloat;

  grf::Tex2D tex = grf.createTex2D(format, width, height);


  auto [w, h] = tex.dims();
  CHECK( w == width );
  CHECK( h == height );

  CHECK( tex.heapIndex() != 0xFFFFFFFF );
  CHECK( tex.size() != 0 );
  CHECK( tex.format() == format );
}

TEST_CASE( "allocation: tex3D", "[allocation][tex3D]" ) {
  grf::GRF grf;
  const uint32_t width = 1024;
  const uint32_t height = 1024;
  const uint32_t depth = 3;
  const grf::Format format = grf::Format::rgba16_sfloat;

  grf::Tex3D tex = grf.createTex3D(format, width, height, depth);


  auto [w, h, d] = tex.dims();
  CHECK( w == width );
  CHECK( h == height );
  CHECK( d == depth );

  CHECK( tex.heapIndex() != 0xFFFFFFFF );
  CHECK( tex.size() != 0 );
  CHECK( tex.format() == format );
}

TEST_CASE( "allocation: cubemap", "[allocation][cubemap]" ) {
  grf::GRF grf;
  const uint32_t width = 1024;
  const uint32_t height = 1024;
  const grf::Format format = grf::Format::rgba16_sfloat;

  grf::Cubemap tex = grf.createCubemap(format, width, height);


  auto [w, h] = tex.dims();
  CHECK( w == width );
  CHECK( h == height );

  CHECK( tex.heapIndex() != 0xFFFFFFFF );
  CHECK( tex.size() != 0 );
  CHECK( tex.format() == format );
}

TEST_CASE( "allocation: img2D", "[allocation][img2D]" ) {
  grf::GRF grf;
  const uint32_t width = 1024;
  const uint32_t height = 1024;
  const grf::Format format = grf::Format::rgba16_sfloat;

  grf::Img2D img = grf.createImg2D(format, width, height);


  auto [w, h] = img.dims();
  CHECK( w == width );
  CHECK( h == height );

  CHECK( img.sampledHeapIndex() != 0xFFFFFFFF );
  CHECK( img.storageHeapIndex() != 0xFFFFFFFF );
  CHECK( img.size() != 0 );
  CHECK( img.format() == format );
}

TEST_CASE( "allocation: img3D", "[allocation][img3D]" ) {
  grf::GRF grf;
  const uint32_t width = 1024;
  const uint32_t height = 1024;
  const uint32_t depth = 3;
  const grf::Format format = grf::Format::rgba16_sfloat;

  grf::Img3D img = grf.createImg3D(format, width, height, depth);


  auto [w, h, d] = img.dims();
  CHECK( w == width );
  CHECK( h == height );
  CHECK( d == depth );

  CHECK( img.sampledHeapIndex() != 0xFFFFFFFF );
  CHECK( img.storageHeapIndex() != 0xFFFFFFFF );
  CHECK( img.size() != 0 );
  CHECK( img.format() == format );
}

TEST_CASE( "allocation: sampler", "[allocation][sampler]" ) {
  grf::GRF grf;
  const grf::SamplerSettings settings{
    .magFilter            = grf::Filter::Linear,
    .minFilter            = grf::Filter::Nearest,
    .uMode                = grf::SampleMode::ClampToBorder,
    .vMode                = grf::SampleMode::ClampToEdge,
    .wMode                = grf::SampleMode::MirroredRepeat,
    .anisotropicFiltering = true
  };

  grf::Sampler sampler = grf.createSampler(settings);

  CHECK(sampler.heapIndex() != 0xFFFFFFFF );
  CHECK( sampler.magFilter() == settings.magFilter );
  CHECK( sampler.minFilter() == settings.minFilter );
  CHECK( sampler.uMode() == settings.uMode );
  CHECK( sampler.vMode() == settings.vMode );
  CHECK( sampler.wMode() == settings.wMode );
  CHECK( sampler.anisotropicFiltering() == settings.anisotropicFiltering );
}