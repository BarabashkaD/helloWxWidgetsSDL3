#pragma once

namespace scene {

// Averages frames over fixed intervals; times are seconds.
class FpsCounter {
public:
    explicit FpsCounter(double interval = 0.5) : interval_(interval) {}

    // Records a frame; returns true when a new fps() value is available.
    bool frame(double now);
    double fps() const { return fps_; }

private:
    double interval_;
    double windowStart_ = -1.0;
    int frames_ = 0;
    double fps_ = 0.0;
};

} // namespace scene
