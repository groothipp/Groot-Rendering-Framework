#include "internal/gsl/assembler.hpp"
#include "internal/gsl/lexer.hpp"
#include "internal/gsl/parser.hpp"
#include "internal/log.hpp"
#include "internal/shader_manager.hpp"

namespace grf {

ShaderManager::ShaderManager(vk::Device& device) : m_device(device) {
  m_opts.SetOptimizationLevel(shaderc_optimization_level_performance);
  m_opts.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
}

ShaderManager::~ShaderManager() {
  for (auto& [path, module] : m_shaders)
    m_device.destroyShaderModule(module);
}

const vk::ShaderModule& ShaderManager::getModule(const Shader& shader) const {
  return m_shaders.at(shader.m_path);
}

Shader ShaderManager::compile(ShaderType type, const std::string& path) {
  if (m_shaders.contains(path)) {
    log::warning("\"{}\" is already compiled", path);
    return Shader();
  }

  std::ifstream file(path);
  if (!file)
    GRF_PANIC("Failed to open \"{}\"", path);

  std::string source = stringify(file);
  std::string shaderSource = getShaderSource(source, path);

  auto res = m_compiler.CompileGlslToSpv(shaderSource, shadercType(type), path.c_str());
  if (res.GetNumErrors() > 0)
    GRF_PANIC("Failed to compile \"{}\":\n{}", path, res.GetErrorMessage());

  if (res.GetNumWarnings() > 0)
    log::warning("\"{}\" compiler warnings:\n{}", path, res.GetErrorMessage());

  std::vector<uint32_t> code(res.begin(), res.end());

  m_shaders.emplace(path, m_device.createShaderModule(vk::ShaderModuleCreateInfo{
    .codeSize = code.size() * sizeof(uint32_t),
    .pCode    = code.data()
  }));

  log::success("Compiled \"{}\"", path);

  return Shader(type, path);
}

std::string ShaderManager::stringify(std::ifstream& file) const {
  std::string source = "", line;
  while (std::getline(file, line))
    source += std::format("{}\n", line);
  return source;
}

std::string ShaderManager::getShaderSource(const std::string& source, const std::string& path) const {
  gsl::Lexer lexer(source);
  auto tokens = lexer.tokenize();

  gsl::Parser parser(tokens, source, path);
  auto parsed = parser.parse();

  gsl::Assembler assembler(parsed);
  return assembler.assemble();
}

shaderc_shader_kind ShaderManager::shadercType(ShaderType type) const {
  switch (type) {
    case ShaderType::Vertex:
      return shaderc_vertex_shader;
    case ShaderType::Fragment:
      return shaderc_fragment_shader;
    case ShaderType::TessCtrl:
      return shaderc_tess_control_shader;
    case ShaderType::TessEval:
      return shaderc_tess_evaluation_shader;
    case ShaderType::Compute:
      return shaderc_compute_shader;
  }
}

}