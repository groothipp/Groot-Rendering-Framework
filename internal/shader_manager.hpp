#pragma once

#include "./structs.hpp"

#include "public/enums.hpp"
#include "public/shader.hpp"

#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.hpp>

#include <unordered_map>
#include <fstream>
#include <random>

namespace grf {

class ShaderManager {
  static constexpr std::string_view g_cacheDir = ".cache/grf";
  static constexpr std::string_view g_binDir = "bin";
  static constexpr std::string_view g_jsonName = "shader_cache.json";

  vk::Device&                                       m_device;
  shaderc::Compiler                                 m_compiler;
  shaderc::CompileOptions                           m_opts;
  std::unordered_map<std::string, vk::ShaderModule> m_shaders;
  std::unordered_map<std::string, CacheRecord>      m_cache;

  std::mt19937_64                                   m_random = std::mt19937_64(std::random_device{}());

public:
  explicit ShaderManager(vk::Device&);
  ~ShaderManager();

  const vk::ShaderModule& getModule(const Shader&) const;
  Shader compile(ShaderType, const std::string&);
  void destroy();

private:
  std::string stringify(std::ifstream&) const;
  std::string getShaderSource(const std::string&, const std::string&) const;
  shaderc_shader_kind shadercType(ShaderType) const;
  std::string generateCacheName();

  void loadCache();
  void saveCache();
  std::vector<uint32_t> codeFromPath(ShaderType, const std::string&);
  std::vector<uint32_t> codeFromCache(const std::string&);
};

}