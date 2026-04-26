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

void applyGreenBrownStyle() {
  ImGui::StyleColorsDark();

  const ImVec4 cream         = ImVec4(0.91f, 0.86f, 0.75f, 1.00f);
  const ImVec4 dimCream      = ImVec4(0.54f, 0.49f, 0.41f, 1.00f);
  const ImVec4 darkBrown     = ImVec4(0.12f, 0.09f, 0.07f, 1.00f);
  const ImVec4 medBrown      = ImVec4(0.18f, 0.13f, 0.09f, 1.00f);
  const ImVec4 hiBrown       = ImVec4(0.24f, 0.17f, 0.12f, 1.00f);
  const ImVec4 darkGreen     = ImVec4(0.18f, 0.28f, 0.13f, 1.00f);
  const ImVec4 medGreen      = ImVec4(0.29f, 0.49f, 0.23f, 1.00f);
  const ImVec4 brightGreen   = ImVec4(0.42f, 0.66f, 0.35f, 1.00f);
  const ImVec4 brighterGreen = ImVec4(0.55f, 0.78f, 0.45f, 1.00f);

  ImVec4 * c = ImGui::GetStyle().Colors;

  c[ImGuiCol_Text]                   = cream;
  c[ImGuiCol_TextDisabled]           = dimCream;
  c[ImGuiCol_WindowBg]               = darkBrown;
  c[ImGuiCol_ChildBg]                = darkBrown;
  c[ImGuiCol_PopupBg]                = ImVec4(darkBrown.x, darkBrown.y, darkBrown.z, 0.94f);
  c[ImGuiCol_Border]                 = darkGreen;
  c[ImGuiCol_BorderShadow]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  c[ImGuiCol_FrameBg]                = medBrown;
  c[ImGuiCol_FrameBgHovered]         = hiBrown;
  c[ImGuiCol_FrameBgActive]          = darkGreen;
  c[ImGuiCol_TitleBg]                = darkGreen;
  c[ImGuiCol_TitleBgActive]          = medGreen;
  c[ImGuiCol_TitleBgCollapsed]       = ImVec4(darkGreen.x, darkGreen.y, darkGreen.z, 0.75f);
  c[ImGuiCol_MenuBarBg]              = hiBrown;
  c[ImGuiCol_ScrollbarBg]            = medBrown;
  c[ImGuiCol_ScrollbarGrab]          = darkGreen;
  c[ImGuiCol_ScrollbarGrabHovered]   = medGreen;
  c[ImGuiCol_ScrollbarGrabActive]    = brightGreen;
  c[ImGuiCol_CheckMark]              = brightGreen;
  c[ImGuiCol_SliderGrab]             = medGreen;
  c[ImGuiCol_SliderGrabActive]       = brightGreen;
  c[ImGuiCol_Button]                 = darkGreen;
  c[ImGuiCol_ButtonHovered]          = medGreen;
  c[ImGuiCol_ButtonActive]           = brightGreen;
  c[ImGuiCol_Header]                 = darkGreen;
  c[ImGuiCol_HeaderHovered]          = medGreen;
  c[ImGuiCol_HeaderActive]           = brightGreen;
  c[ImGuiCol_Separator]              = darkGreen;
  c[ImGuiCol_SeparatorHovered]       = medGreen;
  c[ImGuiCol_SeparatorActive]        = brightGreen;
  c[ImGuiCol_ResizeGrip]             = darkGreen;
  c[ImGuiCol_ResizeGripHovered]      = medGreen;
  c[ImGuiCol_ResizeGripActive]       = brightGreen;
  c[ImGuiCol_Tab]                    = darkGreen;
  c[ImGuiCol_TabHovered]             = brightGreen;
  c[ImGuiCol_TabSelected]            = medGreen;
  c[ImGuiCol_TabSelectedOverline]    = brighterGreen;
  c[ImGuiCol_TabDimmed]              = hiBrown;
  c[ImGuiCol_TabDimmedSelected]      = darkGreen;
  c[ImGuiCol_DockingPreview]         = ImVec4(brightGreen.x, brightGreen.y, brightGreen.z, 0.70f);
  c[ImGuiCol_DockingEmptyBg]         = darkBrown;
  c[ImGuiCol_PlotLines]              = brightGreen;
  c[ImGuiCol_PlotLinesHovered]       = brighterGreen;
  c[ImGuiCol_PlotHistogram]          = medGreen;
  c[ImGuiCol_PlotHistogramHovered]   = brightGreen;
  c[ImGuiCol_TableHeaderBg]          = darkGreen;
  c[ImGuiCol_TableBorderStrong]      = medGreen;
  c[ImGuiCol_TableBorderLight]       = darkGreen;
  c[ImGuiCol_TableRowBg]             = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  c[ImGuiCol_TableRowBgAlt]          = ImVec4(medBrown.x, medBrown.y, medBrown.z, 0.50f);
  c[ImGuiCol_TextSelectedBg]         = ImVec4(brightGreen.x, brightGreen.y, brightGreen.z, 0.35f);
  c[ImGuiCol_DragDropTarget]         = brighterGreen;
  c[ImGuiCol_NavCursor]              = brightGreen;
  c[ImGuiCol_NavWindowingHighlight]  = brighterGreen;
  c[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.0f, 0.0f, 0.0f, 0.40f);
  c[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);
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

  applyGreenBrownStyle();

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
