#include "render/Log.h"

#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace render {

void log(const char* message)
{
    std::fprintf(stderr, "%s\n", message);
    std::fflush(stderr);
#ifdef _WIN32
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
#endif
}

} // namespace render
