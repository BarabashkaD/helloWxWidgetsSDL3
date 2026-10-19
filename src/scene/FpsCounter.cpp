#include "scene/FpsCounter.h"

namespace scene {

bool FpsCounter::frame(double now)
{
    if (windowStart_ < 0.0) {
        windowStart_ = now;
        return false;
    }
    ++frames_;
    const double elapsed = now - windowStart_;
    if (elapsed < interval_) {
        return false;
    }
    fps_ = frames_ / elapsed;
    frames_ = 0;
    windowStart_ = now;
    return true;
}

} // namespace scene
