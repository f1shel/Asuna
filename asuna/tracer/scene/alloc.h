#pragma once

#include <cassert>

class GPUAlloc
{
  public:
    GPUAlloc() : m_released(false)
    {}
    // Make sure gpu resource was released before calling this method
    void intoReleased()
    {
        m_released = true;
    }
    ~GPUAlloc()
    {
        assert(m_released == true, "every resource which was allocated on GPU should call "
                                   "deinit() before it was deconstructed!");
    }

  protected:
    bool m_released{true};
};