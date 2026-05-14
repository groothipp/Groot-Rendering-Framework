#pragma once

#include <memory>

namespace grf {

class CommandBuffer;

class GUI {
  friend class GRF;

public:
  class Impl;

  ~GUI();
  GUI(const GUI&)            = delete;
  GUI& operator=(const GUI&) = delete;
  GUI(GUI&&)                 = delete;
  GUI& operator=(GUI&&)      = delete;

  void beginFrame();
  void render(CommandBuffer&);
  void renderPlatformWindows();

  bool wantsMouse() const;
  bool wantsKeyboard() const;

private:
  std::unique_ptr<Impl> m_impl;
  explicit GUI(std::unique_ptr<Impl>);
};

}
