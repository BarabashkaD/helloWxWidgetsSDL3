#include <doctest/doctest.h>

#include "scene/FpsCounter.h"

using scene::FpsCounter;

TEST_CASE("first frame only starts the measurement")
{
    FpsCounter counter;
    CHECK_FALSE(counter.frame(0.0));
    CHECK(counter.fps() == 0.0);
}

TEST_CASE("reports frames per second once the interval has elapsed")
{
    FpsCounter counter(0.5);
    counter.frame(0.0);
    for (int i = 1; i <= 4; ++i) {
        CHECK_FALSE(counter.frame(0.1 * i));
    }
    CHECK(counter.frame(0.5));
    CHECK(counter.fps() == doctest::Approx(10.0));
}

TEST_CASE("starts a new interval after reporting")
{
    FpsCounter counter(0.5);
    counter.frame(0.0);
    counter.frame(0.5);
    CHECK_FALSE(counter.frame(0.75));
    CHECK(counter.frame(1.0));
    CHECK(counter.fps() == doctest::Approx(4.0));
}
