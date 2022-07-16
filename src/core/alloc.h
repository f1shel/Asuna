#pragma once

#include <cassert>

class GpuAlloc {
public:
  GpuAlloc() : m_released(false) {}

  // Make sure gpu resource was released before calling this method
  void intoReleased() { m_released = true; }

  ~GpuAlloc() {
    assert((m_released == true,
            // clang-format off
        "every resource which was allocated on GPU should call deinit() before it was deconstructed!"));
    // clang-format on
  }

protected:
  bool m_released{true};
};