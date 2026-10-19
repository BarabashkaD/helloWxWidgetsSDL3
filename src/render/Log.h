#pragma once

namespace render {

// Writes a diagnostic line to stderr (and the debugger output on Windows).
void log(const char* message);

} // namespace render
