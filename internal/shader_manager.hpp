#pragma once

#include "public/enums.hpp"
#include "public/shader.hpp"

#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.hpp>

#include <unordered_map>
#include <fstream>

namespace grf {

class ShaderManager {
  vk::Device&                                       m_device;
  shaderc::Compiler                                 m_compiler;
  shaderc::CompileOptions                           m_opts;
  std::unordered_map<std::string, vk::ShaderModule> m_shaders;

public:
  explicit ShaderManager(vk::Device&);
  ~ShaderManager();

  const vk::ShaderModule& getModule(const Shader&) const;
  Shader compile(ShaderType, const std::string&);

private:
  std::string stringify(std::ifstream&) const;
  std::string getShaderSource(const std::string&, const std::string&) const;
  shaderc_shader_kind shadercType(ShaderType) const;
};

}