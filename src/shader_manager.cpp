#include "internal/gsl/assembler.hpp"
#include "internal/gsl/lexer.hpp"
#include "internal/gsl/parser.hpp"
#include "internal/log.hpp"
#include "internal/shader_manager.hpp"
#include "internal/structs.hpp"

#include "external/nlohmann/json.hpp"

#include <filesystem>

using json = nlohmann::json;

namespace grf {

ShaderManager::ShaderManager(vk::Device& device) : m_device(device) {
  m_opts.SetOptimizationLevel(shaderc_optimization_level_performance);
  m_opts.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
  loadCache();
}

ShaderManager::~ShaderManager() {
  for (auto& [path, module] : m_shaders)
    m_device.destroyShaderModule(module);
  saveCache();
}

const vk::ShaderModule& ShaderManager::getModule(const Shader& shader) const {
  return m_shaders.at(shader.m_path);
}

Shader ShaderManager::compile(ShaderType type, const std::string& path) {
  if (m_shaders.contains(path)) {
    log::warning("\"{}\" is already compiled", path);
    return Shader();
  }

  if (!std::filesystem::exists(path))
    GRF_PANIC("\"{}\" does not exist", path);

  uint64_t mtime = std::filesystem::last_write_time(path).time_since_epoch().count();
  uint64_t size = std::filesystem::file_size(path);

  auto itr = m_cache.find(path);
  bool staleCache =
    itr == m_cache.end()        ||
    itr->second.mtime != mtime  ||
    itr->second.size != size;

  std::vector<uint32_t> code = staleCache ? codeFromPath(type, path) : codeFromCache(path);

  bool corrupted = !staleCache && code.empty();
  if (corrupted) code = codeFromPath(type, path);

  if (itr != m_cache.end() && (staleCache || corrupted)) {
    std::string binPath = std::filesystem::path(g_cacheDir) / g_binDir / itr->second.bin;
    if (std::filesystem::exists(binPath))
      std::filesystem::remove(binPath);
  }

  m_shaders.emplace(path, m_device.createShaderModule(vk::ShaderModuleCreateInfo{
    .codeSize = code.size() * sizeof(uint32_t),
    .pCode    = code.data()
  }));

  if (staleCache || corrupted) {
    std::string cacheName = generateCacheName();

    std::filesystem::create_directories(std::filesystem::path(g_cacheDir) / g_binDir);
    std::ofstream file(std::filesystem::path(g_cacheDir) / g_binDir / cacheName, std::ios::binary);
    file.write(reinterpret_cast<const char *>(code.data()), code.size() * sizeof(uint32_t));

    m_cache[path] = CacheRecord{
      .mtime  = mtime,
      .size   = size,
      .bin    = cacheName
    };
  }

  std::string msg = (staleCache || corrupted) ?
    std::format("Compiled {}", path) : std::format("Retrieved {} from cache", path);
  log::success("{}", msg);

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

std::string ShaderManager::generateCacheName() {
  uint64_t hi = m_random();
  uint64_t lo = m_random();
  return std::format("{:016x}{:016x}.spv", hi, lo);
}

void ShaderManager::loadCache() {
  std::ifstream file(std::filesystem::path(g_cacheDir) / g_jsonName);
  if (!file) return;

  json j;
  try { file >> j; }
  catch (const json::parse_error&) { return; }

  if (j.value<std::string>("version", "0") != GRF_VERSION) {
    std::filesystem::remove_all(std::filesystem::path(g_cacheDir) / g_binDir);
    log::warning("Cleared cache due to version mismatch");
    return;
  }

  if (!j.contains("entries")) return;

  for (const auto& [path, record] : j.at("entries").items()) {
    m_cache[path] = CacheRecord{
      .mtime  = record.at("mtime").get<uint64_t>(),
      .size   = record.at("size").get<uint64_t>(),
      .bin    = record.at("bin").get<std::string>()
    };
  }
}

void ShaderManager::saveCache() {
  json j;
  j["version"] = GRF_VERSION;
  for (const auto& [path, record] : m_cache) {
    j["entries"][path] = {
      { "mtime", record.mtime },
      { "size", record.size },
      { "bin", record.bin }
    };
  }

  std::filesystem::create_directories(g_cacheDir);
  std::ofstream file(std::filesystem::path{g_cacheDir} / g_jsonName);
  file << j.dump(2);
}

std::vector<uint32_t> ShaderManager::codeFromPath(ShaderType type, const std::string& path) {
  std::ifstream file(path);

  std::string source = stringify(file);
  std::string shaderSource = getShaderSource(source, path);

  auto res = m_compiler.CompileGlslToSpv(shaderSource, shadercType(type), path.c_str());
  if (res.GetNumErrors() > 0)
    GRF_PANIC("Failed to compile \"{}\":\n{}", path, res.GetErrorMessage());

  if (res.GetNumWarnings() > 0)
    log::warning("\"{}\" compiler warnings:\n{}", path, res.GetErrorMessage());

  std::vector<uint32_t> code(res.begin(), res.end());
  return code;
}

std::vector<uint32_t> ShaderManager::codeFromCache(const std::string& path) {
  const std::string binPath = std::filesystem::path(g_cacheDir) / g_binDir / m_cache.at(path).bin;

  std::ifstream file(binPath, std::ios::binary | std::ios::ate);
  if (!file) {
    log::warning("Corrupted cache. Recompiling {}", path);
    return {};
  }

  auto size = file.tellg();
  file.seekg(0);

  std::vector<uint32_t> code(size / sizeof(uint32_t));
  file.read(reinterpret_cast<char *>(code.data()), size);

  return code;
}

}