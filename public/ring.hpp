#pragma once

#include <vector>

namespace grf {

template <typename T>
class Ring {
  friend class GRF;

  std::vector<T> m_objs;

public:
  Ring() = default;

  T& operator[](uint32_t index) {
    return m_objs[index];
  }

  const T& operator[](uint32_t index) const {
    return m_objs[index];
  }

private:
  explicit Ring(uint32_t count) { m_objs.reserve(count); };
};

}