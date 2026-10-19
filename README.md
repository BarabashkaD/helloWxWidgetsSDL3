# wxWidgets + SDL3 + Vulkan spinning cube

A small cross-platform desktop application. A wxWidgets main window hosts a native child panel; SDL3 wraps that panel as a Vulkan-capable window, and a Vulkan 1.1 renderer (classic `VkRenderPass`, FIFO present, 2 frames in flight) draws a coloured cube that spins on its own. The window has a Start/Stop button and a status bar.

## Features

- Vulkan-rendered cube, 90 deg/s initial spin about `normalize(1,1,0)`.
- Start/Stop button (label shows the action: `Stop` while spinning, `Start` while stopped).
- Status bar: `FPS: <value>` (refreshed every 0.5 s) and `Spinning` / `Stopped`.
- Mouse drag rotates the cube; a quick flick (pointer speed over the last 100 ms before release above 50 px/s) changes the spin axis.
- Mouse wheel zooms toward the cube (x0.9 per notch, distance clamped to 1.8 .. 10.0, default 3.0).
- Window can be freely resized, maximized, minimized and restored.
- All dependencies built by `deps/` are linked statically; the Vulkan loader (and GTK/X11 on Linux) are system shared libraries. The MSVC runtime is the default `/MD` / `/MDd`.

## Prerequisites

Common: CMake >= 3.24, a C++20 compiler, and Vulkan >= 1.3.275 headers, loader and `glslc` (from the Vulkan SDK on Windows and macOS, from distro packages on Linux).

- **Windows:** Visual Studio 2022 or 2026 with the "Desktop development with C++" workload, and the LunarG Vulkan SDK. Run the commands from PowerShell.
- **Linux:** GCC or Clang with C++20, CMake, Ninja and pkg-config, plus the GTK 3, X11 and Vulkan development packages. The Vulkan SDK is not needed. On Ubuntu 24.04 / Debian:

  ```sh
  sudo apt-get install -y build-essential cmake ninja-build pkg-config \
      libgtk-3-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
      libxss-dev libxfixes-dev libxtst-dev libxkbcommon-dev \
      libvulkan-dev glslc
  ```

  Optional: `vulkan-tools` (`vulkaninfo`, to check the driver) and `vulkan-validationlayers` (validation in Debug builds). This package list was verified by a full build (dependencies, Release and Debug) on Ubuntu 24.04 with GCC 13.
- **macOS:** Xcode command line tools, Ninja, LunarG Vulkan SDK with MoltenVK (not yet supported, see the status table).

## Build

The dependencies (wxWidgets, SDL3, vk-bootstrap, VMA, GLM, ...) are built once by the superbuild in `deps/` and installed to `deps/install/<preset>`. The application finds them only through `find_package` and `CMAKE_PREFIX_PATH`.

Windows (PowerShell):

```powershell
cmake -S deps --preset vs2022
cmake --build deps/build/vs2022 --config Debug
cmake --build deps/build/vs2022 --config Release
cmake --preset vs2022
cmake --build --preset vs2022-debug
ctest --preset vs2022-debug
.\build\vs2022\Debug\cubeapp.exe
```

Use the `vs2026` presets for Visual Studio 2026.

Linux / macOS (the Ninja presets are not available on Windows):

```sh
cmake -S deps --preset ninja
cmake --build deps/build/ninja
cmake --preset ninja-release
cmake --build --preset ninja-release
ctest --preset ninja-release
./build/ninja-release/cubeapp            # macOS: ./build/ninja-release/cubeapp.app/Contents/MacOS/cubeapp (source the Vulkan SDK setup-env.sh first)
```

On Linux the application forces the X11 backends (`GDK_BACKEND=x11`, `SDL_VIDEO_DRIVER=x11`) unless you have set them yourself, so it also runs under Wayland through XWayland.

## Controls

| Input | Action |
| --- | --- |
| Start / Stop button | Pause or resume the spin |
| Left-button drag in the render area | Rotate the cube by 0.01 rad per pixel |
| Fast drag and release (flick) | Spin continues about the axis given by the flick direction |
| Mouse wheel | Zoom in / out toward the cube |
| Resize / maximize / minimize | Swapchain is recreated; the cube keeps its proportions |

## Using system packages instead of `deps/`

Skip the superbuild and point CMake at your own installs of the dependencies:

```sh
cmake --preset ninja-release -DCMAKE_PREFIX_PATH="/path/to/wxWidgets;/path/to/SDL3;/path/to/other/prefixes"
```

Every dependency must be discoverable with `find_package` and built as a static library.

## Platform status

| Platform | Status |
| --- | --- |
| Windows 11 + Visual Studio 2022 | Verified (build, unit tests, run, interaction, validation layer clean) |
| Linux X11 / XWayland | Verified on Ubuntu 24.04 under WSL2 (WSLg XWayland, Mesa lavapipe CPU Vulkan) with GCC 13: build (Release and Debug), unit tests, run, interaction. Not yet tested on native X11 with a hardware GPU or with the validation layer |
| macOS | Not yet supported or verified: a known SDL Cocoa embedding issue (SDL window inside a wxWidgets native view) has still to be addressed |
