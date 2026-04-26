#include "internal/gui.hpp"
#include "internal/cmd.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

namespace grf {

namespace {

vk::DescriptorPool createImGuiDescriptorPool(vk::Device device) {
  constexpr uint32_t kPoolSize = 1024;
  std::array<vk::DescriptorPoolSize, 1> sizes{{
    { vk::DescriptorType::eCombinedImageSampler, kPoolSize }
  }};

  return device.createDescriptorPool(vk::DescriptorPoolCreateInfo{
    .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
    .maxSets       = kPoolSize,
    .poolSizeCount = static_cast<uint32_t>(sizes.size()),
    .pPoolSizes    = sizes.data()
  });
}

}

GUI::Impl::Impl(
  GLFWwindow *       window,
  vk::Instance       instance,
  vk::PhysicalDevice gpu,
  vk::Device         device,
  const Queue&       graphicsQueue,
  vk::Format         colorFormat,
  uint32_t           imageCount
) : m_device(device), m_window(window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForVulkan(window, true);

  m_descriptorPool = createImGuiDescriptorPool(device);

  m_colorFormat = static_cast<VkFormat>(colorFormat);

  ImGui_ImplVulkan_InitInfo info{};
  info.ApiVersion          = VK_API_VERSION_1_4;
  info.Instance            = instance;
  info.PhysicalDevice      = gpu;
  info.Device              = device;
  info.QueueFamily         = graphicsQueue.index;
  info.Queue               = graphicsQueue.queue;
  info.DescriptorPool      = m_descriptorPool;
  info.MinImageCount       = imageCount;
  info.ImageCount          = imageCount;
  info.PipelineCache       = VK_NULL_HANDLE;
  info.UseDynamicRendering = true;

  info.PipelineInfoMain.MSAASamples                                = VK_SAMPLE_COUNT_1_BIT;
  info.PipelineInfoMain.PipelineRenderingCreateInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount    = 1;
  info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_colorFormat;

  ImGui_ImplVulkan_Init(&info);
}

GUI::Impl::~Impl() {
  m_device.waitIdle();
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  m_device.destroyDescriptorPool(m_descriptorPool);
}

void GUI::Impl::beginFrame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
}

GUI::GUI(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}
GUI::~GUI() = default;

void GUI::render(CommandBuffer& cmd) {
  ImGui::Render();
  ImDrawData* drawData = ImGui::GetDrawData();
  if (drawData == nullptr || drawData->CmdListsCount == 0) return;
  ImGui_ImplVulkan_RenderDrawData(drawData, cmd.m_impl->m_buffer);
}

bool GUI::wantsMouse() const {
  return ImGui::GetIO().WantCaptureMouse;
}

bool GUI::wantsKeyboard() const {
  return ImGui::GetIO().WantCaptureKeyboard;
}

}
