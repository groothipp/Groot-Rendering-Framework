#include "public/grf.hpp"

#include "external/imgui/imgui.h"

#include <cmath>
#include <format>

using namespace grf;

struct PC {
  vec2  pos1;
  vec2  pos2;
  vec2  lightDir;
  uvec2 screenDims;
  u32   texIndex;
  u32   samplerIndex;
  f32   liquidness;
  f32   radius1;
  f32   radius2;
  f32   blur;
};

static void LightDial(const char* label, f32* angle, f32 size = 80.0f) {
  ImGui::PushID(label);

  ImDrawList* draw      = ImGui::GetWindowDrawList();
  ImVec2      origin    = ImGui::GetCursorScreenPos();
  ImVec2      center    = ImVec2(origin.x + size * 0.5f, origin.y + size * 0.5f);
  f32         radius    = size * 0.5f;

  ImGui::InvisibleButton("##hit", ImVec2(size, size));

  if (ImGui::IsItemActive()) {
    ImVec2 mouse = ImGui::GetIO().MousePos;
    f32    dx    = mouse.x - center.x;
    f32    dy    = mouse.y - center.y;
    *angle       = std::atan2(-dy, dx);
  }

  draw->AddCircle(center, radius - 2.0f, IM_COL32(180, 180, 180, 220), 0, 2.0f);

  f32    vx  = std::cos(*angle);
  f32    vy  = -std::sin(*angle);
  ImVec2 tip = ImVec2(center.x + vx * (radius - 8.0f), center.y + vy * (radius - 8.0f));
  draw->AddLine(center, tip, IM_COL32(255, 220, 100, 255), 3.0f);
  draw->AddCircleFilled(tip, 5.0f, IM_COL32(255, 220, 100, 255));
  draw->AddCircleFilled(center, 3.0f, IM_COL32(150, 150, 150, 255));

  ImGui::SameLine();
  ImGui::Text("%s: %.0f\xc2\xb0", label, *angle * 180.0f / 3.14159265f);

  ImGui::PopID();
}

const u32 g_windowWidth = 1280;
const u32 g_windowHeight = 720;

int main() {
  GRF grf(Settings{
    .windowTitle  = "GRF Example: Glass Bubbles",
    .windowSize   = { g_windowWidth, g_windowHeight }
  });

  Shader vertShader = grf.compileShader(ShaderType::Vertex, std::format("{}/vert.gsl", SHADERS));
  Shader fragShader = grf.compileShader(ShaderType::Fragment, std::format("{}/frag.gsl", SHADERS));

  BlendState alphaBlend{
    .enable         = true,
    .srcColorFactor = BlendFactor::SrcAlpha,
    .dstColorFactor = BlendFactor::OneMinusSrcAlpha
  };

  GraphicsPipeline pipeline = grf.createGraphicsPipeline(vertShader, fragShader, GraphicsPipelineSettings{
    .colorFormats = { Format::bgra8_srgb },
    .cullMode     = CullMode::None,
    .blends       = { alphaBlend }
  });

  Tex2D background;
  {
    ImageData tex = readImage(std::format("{}/background.png", ASSETS)).get();
    background = grf.createTex2D(tex.format, tex.width, tex.height);
    background.write(tex.bytes, Layout::ShaderReadOptimal);
  }

  Sampler sampler = grf.createSampler(SamplerSettings{
    .uMode                = SampleMode::ClampToBorder,
    .vMode                = SampleMode::ClampToBorder,
    .anisotropicFiltering = false
  });

  Ring<CommandBuffer> cmdRing = grf.createCmdRing(QueueType::Graphics);
  Ring<Fence> flightFenceRing = grf.createFenceRing(true);
  Ring<Semaphore> imgSemRing = grf.createSemaphoreRing();
  Ring<Semaphore> drawSemRing = grf.createSemaphoreRing();

  f32 liquidness = 0.15;
  f32 radius1 = 0.3;
  f32 radius2 = 0.3;
  f32 blur = 0.8;
  f32 lightAngle = 3.14159265f * 0.5f;
  vec2 pos1 = vec2(-0.2, 0.0);
  vec2 pos2 = vec2(0.2, 0.0);

  Input& input = grf.input();
  while (grf.running([&](){ return input.isJustPressed(Key::Escape); })) {
    auto [frameIndex, _] = grf.beginFrame();

    grf.gui().beginFrame();
    grf.profiler().render();

    ImGui::Begin("Settings");
      ImGui::Text("Click and drag the bubbles to move them");
      ImGui::SliderFloat("Liquidess", &liquidness, 0.01, 1.0, "%.2f");
      ImGui::SliderFloat("Radius 1", &radius1, 0.1, 1.0, "%.1f");
      ImGui::SliderFloat("Radius 2", &radius2, 0.1, 1.0, "%.1f");
      ImGui::SliderFloat("Blur",     &blur,     0.0, 1.0, "%.2f");
      LightDial("Light", &lightAngle);
    ImGui::End();

    auto [screenW, screenH] = grf.screenDims();

    if (!grf.gui().wantsMouse() && input.isPressed(MouseButton::Left)) {
      const f32 ar = static_cast<f32>(screenW) / static_cast<f32>(screenH);

      const auto circleSDF = [&](vec2 pos, vec2 center, f32 r) {
        return glm::distance(pos, center) - r;
      };

      const auto smin = [&](f32 a, f32 b, f32 k){
        float h = std::max(k - abs(a - b), 0.0f) / k;
        return std::min(a, b) - h * h * k * 0.25;
      };

      auto [x, y] = input.cursorPos();
      vec2 cursor = vec2(ar * static_cast<f32>(x), static_cast<f32>(y));

      f32 sdf1 = circleSDF(cursor, pos1, radius1);
      f32 sdf2 = circleSDF(cursor, pos2, radius2);
      f32 blended = smin(sdf1, sdf2, liquidness);

      if (sdf1 < 0.0 || sdf2 < 0.0) {
        vec2& pos = sdf1 < sdf2 ? pos1 : pos2;
        pos = cursor;
      }
    }

    auto& cmd = cmdRing[frameIndex];
    auto& flightFence = flightFenceRing[frameIndex];
    auto& imgSem = imgSemRing[frameIndex];
    auto& drawSem = drawSemRing[frameIndex];

    grf.waitFences({ flightFence });
    grf.resetFences({ flightFence });

    SwapchainImage swapchainImage = grf.nextSwapchainImage(imgSem);
    ColorAttachment swapchainAttachment{
      .img      = swapchainImage,
      .loadOp   = LoadOp::Clear,
      .storeOp  = StoreOp::Store
    };

    cmd.begin();
    cmd.transition(swapchainImage, Layout::Undefined, Layout::ColorAttachmentOptimal);
    cmd.beginRendering({ swapchainAttachment });

    cmd.beginProfile("draw");
    cmd.bindPipeline(pipeline);
    cmd.push(PC{
      .pos1         = pos1,
      .pos2         = pos2,
      .lightDir     = vec2(std::cos(lightAngle), -std::sin(lightAngle)),
      .screenDims   = uvec2(screenW, screenH),
      .texIndex     = background.heapIndex(),
      .samplerIndex = sampler.heapIndex(),
      .liquidness   = liquidness,
      .radius1      = radius1,
      .radius2      = radius2,
      .blur         = blur
    });
    cmd.draw(6);
    cmd.endProfile();

    cmd.beginProfile("gui");
    grf.gui().render(cmd);
    cmd.endProfile();

    cmd.endRendering();
    cmd.transition(swapchainImage, Layout::ColorAttachmentOptimal, Layout::PresentSrc);
    cmd.end();

    grf.waitForResourceUpdates();

    grf.submit(cmd, { imgSem }, { drawSem }, flightFence);
    grf.present(swapchainImage, { drawSem });

    grf.endFrame();
  }
}