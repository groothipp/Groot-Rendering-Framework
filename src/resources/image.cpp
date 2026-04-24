#include "internal/resources/image.hpp"
#include "internal/resources/resource_manager.hpp"

namespace grf {

Image::Image(std::unique_ptr<ResourceManager>& resourceManager, const ImageInfo& info)
: m_resourceManager(resourceManager),
  m_id(info.id),
  m_allocation(info.alloc),
  m_image(info.image),
  m_format(info.format),
  m_size(info.size),
  m_width(info.width),
  m_height(info.height),
  m_depth(info.depth)
{}

}