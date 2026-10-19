# wxWidgets + SDL3 + Vulkan Spinning Cube — Design

Date: 2026-10-19
Status: Approved design, pending implementation plan

## 1. Goal

A cross-platform desktop application (Windows, Linux, macOS) built from scratch with CMake
(Visual Studio on Windows). A wxWidgets main window hosts an SDL3-managed native child window
in which a colored cube is rendered with Vulkan.

Functional requirements:

- Colored cube rendered with Vulkan, spinning around an axis.
- Bottom bar of the main window has a Start/Stop toggle button controlling the spin.
- Status bar shows rendered frames per second and spin state (`Spinning` / `Stopped`).
- Mouse drag changes the spin direction (drag-and-flick).
- Mouse wheel zooms in/out within limits.
- Window resize is supported (including maximize and minimize/restore).

## 2. Decisions

| Topic | Decision |
|---|---|
| Rendering approach | Direct Vulkan; SDL3 provides window wrapping, Vulkan surface, and Vulkan library loading |
| Vulkan helpers | vk-bootstrap (instance, device, queues, swapchain), Vulkan Memory Allocator (memory), plain Vulkan C API |
| Math | glm |
| macOS | Vulkan via MoltenVK (LunarG Vulkan SDK required to build and run) |
| Linux | X11 only; Wayland sessions run through XWayland |
| Dependencies | Built from source by a separate CMake superbuild into a repo-local prefix, consumed via `find_package` |
| Linking | All third-party libraries static; single executable |
| Mouse drag | Drag-and-flick: direct trackball rotation while dragging, flick sets new spin axis |
| Testing | doctest unit tests for pure logic; manual verification of GUI/rendering on Windows |

Rejected alternatives:

- SDL3 GPU API forced to Vulkan: its Vulkan driver is not supported on macOS (Metal only), which
  conflicts with the MoltenVK decision.
- SDL3 2D renderer with `SDL_RenderGeometry`: no depth buffer; 3D would be faked with CPU
  projection and face sorting.
- Vulkan-Hpp RAII instead of vk-bootstrap: does not simplify instance/device/swapchain setup.

## 3. Repository Layout

```text
helloWxWidgetsSDL3/
  deps/
    CMakeLists.txt        superbuild: wxWidgets, SDL3, vk-bootstrap, VMA, glm, doctest
    versions.cmake        pinned versions, URLs, SHA-256 hashes
    CMakePresets.json     dependency builds per toolchain (vs2022, vs2026, ninja)
  CMakeLists.txt
  CMakePresets.json
  shaders/
    cube.vert
    cube.frag
  src/
    app/        App (wxApp), MainFrame, RenderPanel, cubeapp.rc (Windows manifest)
    platform/   embed.h, embed_detail.h, embed_common.cpp, embed_win32.cpp, embed_cocoa.mm, embed_x11.cpp
    render/     Log, VkCheck, VulkanContext, Swapchain, CubePipeline, CubeMesh, Renderer, vma_impl.cpp
    scene/      SpinController, Camera, FpsCounter, CubeGeometry
  tests/        doctest tests for scene/
  docs/superpowers/specs/
  README.md
  .gitignore
```

## 4. Architecture

### 4.1 Units and responsibilities

- `app/App` — wxApp entry point; creates `MainFrame`.
- `app/MainFrame` — top-level window. Contains `RenderPanel` (expanding), a bottom bar with the
  Start/Stop toggle button, and a two-field status bar (`FPS: 143.9` | `Spinning`/`Stopped`).
  Catches renderer exceptions and reports them.
- `app/RenderPanel` — native child `wxWindow`. Owns the embedded `SDL_Window*`, the `Renderer`,
  `SpinController`, `Camera`, and `FpsCounter`. Translates wx mouse, wheel, size, and idle events
  into scene/renderer calls.
- `platform/embed.h` — single function `SDL_Window* embedNativeWindow(wxWindow&)`. Implementations:
  - Windows: `HWND` from `GetHandle()` → `SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER`.
  - macOS: `NSView*` from `GetHandle()` → `SDL_PROP_WINDOW_CREATE_COCOA_VIEW_POINTER`.
  - Linux: X11 window ID from the widget's realized native `GdkWindow` (`gdk_x11_window_get_xid`)
    → `SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER`.
  - All set `SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN`. This is the only platform-specific code.
- `render/` — pure Vulkan, no wxWidgets dependency. Inputs: `SDL_Window*`, pixel size, MVP matrix.
  - `VulkanContext` — instance (SDL-required extensions via `SDL_Vulkan_GetInstanceExtensions`),
    debug messenger in Debug builds, surface (`SDL_Vulkan_CreateSurface`), physical device
    selection, logical device, graphics/present queues, VMA allocator.
  - `Swapchain` — vk-bootstrap swapchain, image views, depth image (VMA), render pass, framebuffers;
    `recreate(width, height)` passes the old swapchain.
  - `CubePipeline` — shader modules from embedded SPIR-V, pipeline layout with a 64-byte push
    constant (MVP), graphics pipeline.
  - `CubeMesh` — vertex and index buffers (VMA).
  - `Renderer` — frames in flight, command recording, submit, present, swapchain rebuild handling.
    Public interface: `resize(width, height)`, `draw(const glm::mat4& mvp)`, `waitIdle()`.
- `scene/` — pure logic, no wxWidgets or Vulkan dependency; unit-tested.
  - `SpinController` — orientation, spin axis/speed, start/stop, drag, flick.
  - `Camera` — zoom distance and projection.
  - `FpsCounter` — frame counting with an injectable clock.
  - `CubeGeometry` — vertex/index data of the cube (unit-tested for outward winding).

### 4.2 Event and data flow

- wxWidgets owns the main loop. Input is handled through wxWidgets events on `RenderPanel`;
  SDL events are not used.
- Frame tick: `wxEVT_IDLE` on `RenderPanel` calls `tick()` and requests more idle events.
  `tick()`:
  1. Computes frame time.
  2. `SpinController::update(dt)`.
  3. Builds MVP from `SpinController` orientation and `Camera`.
  4. `Renderer::draw(mvp)`.
  5. `FpsCounter::frame()`; status bar is refreshed when the counter produces a new value.
- With FIFO present mode, presentation blocks on vsync, so FPS matches the display refresh rate.
- `wxEVT_SIZE` → `Renderer::resize` with the size in physical pixels (client size × content
  scale factor) and `Camera` aspect update.
- `wxEVT_SIZE` and `wxEVT_PAINT` also render a frame, keeping the view live during the Windows
  modal resize loop. No frames are rendered (and no idle events requested) while the top-level
  window is iconized.
- The renderer is created lazily on the first frame tick once the panel is shown on screen with a
  non-zero size (on GTK the widget must be realized first).
- SDL queues events for the wrapped window that the app never reads; the queue is cleared with
  `SDL_FlushEvents` (which does not pump the platform message loop) after each frame.
- Linux startup: in the `wxApp` constructor (which runs before GTK initializes), set
  `GDK_BACKEND=x11` and `SDL_VIDEO_DRIVER=x11` unless already set by the user. SDL is told not to
  select input on the foreign X11 window (`SDL_HINT_VIDEO_X11_EXTERNAL_WINDOW_INPUT=0`) so
  wxWidgets keeps receiving mouse events.

## 5. Rendering Details

- Vulkan API version 1.1 minimum. Classic `VkRenderPass` with one color and one depth attachment.
- vk-bootstrap instance: application name, Vulkan 1.1, SDL-required extensions. In Debug builds,
  request the Khronos validation layer if available and route messages to the wx debug log or
  stderr; if unavailable, log one line and continue.
- macOS/MoltenVK: portability enumeration and portability subset enabled (vk-bootstrap handles
  enumeration; device extension enabled when present).
- Physical device: must support graphics and presenting to the surface; prefer discrete GPU.
- Swapchain: prefer `B8G8R8A8_SRGB` with `SRGB_NONLINEAR`, fall back to the first supported
  format; present mode FIFO.
- Depth: `D32_SFLOAT` (fallbacks `D32_SFLOAT_S8_UINT`, `D24_UNORM_S8_UINT`, `D16_UNORM`),
  allocated through VMA, recreated with the swapchain.
- Cube: 24 vertices (`vec3 position`, `vec3 color`), 36 indices, unit cube centered at origin
  (half-extent 0.5). Face colors: red, green, blue, yellow, cyan, magenta.
- Shaders: vertex shader multiplies position by push-constant MVP and passes color; fragment
  shader outputs color. Back-face culling, counter-clockwise front faces, depth test and write.
- Clear color: dark gray.
- Frames in flight: 2. Per frame: command buffer, image-acquired semaphore, in-flight fence.
  Per swapchain image: render-finished semaphore.
- Swapchain rebuild triggers: `Renderer::resize`, `VK_ERROR_OUT_OF_DATE_KHR` or
  `VK_SUBOPTIMAL_KHR` from acquire/present. Rebuild waits for device idle first.
- Zero-size surface (minimized): skip drawing until a non-zero size arrives; a rebuild is deferred
  while the surface's current extent is 0×0.
- Shaders are compiled by `glslc -mfmt=c` at build time into C initializer lists that are
  `#include`d into `uint32_t` arrays; no runtime shader files.

## 6. Interaction Details

- Orientation: `glm::quat`. Initial spin axis: normalized (1, 1, 0). Spin speed: 90°/s
  (π/2 rad/s). Application starts spinning.
- Spinning: each update, `orientation = angleAxis(speed * dt, axis) * orientation` (world-space
  axis).
- Drag (left button held):
  - Automatic spin is paused for the duration of the drag.
  - Mouse is captured so dragging outside the panel continues to work.
  - Mouse delta (dx, dy) in pixels rotates the cube about the world axis `normalize(dy, dx, 0)`
    by `0.01 rad × |delta|`.
- Flick (left button released):
  - Drag velocity is averaged over the last 100 ms of drag samples.
  - If velocity magnitude > 50 px/s: new spin axis = `normalize(vy, vx, 0)`. Speed is unchanged.
  - Otherwise the previous axis is kept.
  - Flick never changes the running/stopped state; if stopped, the new axis applies on the next
    Start.
- Zoom: each wheel notch multiplies camera distance by 0.9 (wheel away from user, zoom in) or
  1/0.9 (wheel toward user, zoom out);
  fractional wheel rotation (touchpads) scales the exponent proportionally. Distance clamped to
  [1.8, 10.0]; default 3.0.
- Projection: perspective, 45° vertical FOV, near 0.1, far 100, aspect from panel pixel size,
  depth range [0, 1], Y flipped for Vulkan clip space. Height 0 is guarded (aspect falls back to 1).
- Start/Stop: one button; label `Stop` while spinning, `Start` while stopped.
- Status bar:
  - Field 1: `FPS: <value with one decimal>`, computed as frames / elapsed time, refreshed every
    0.5 s.
  - Field 2: `Spinning` or `Stopped`, reflecting the Start/Stop state (not the temporary pause
    during a drag).

## 7. Build System

### 7.1 Dependency superbuild (`deps/`)

- `ExternalProject_Add` per dependency, downloading release archives verified by pinned SHA-256.
- Versions recorded in `deps/versions.cmake`:
  - wxWidgets 3.2.9 (latest 3.2.x).
  - SDL 3.2.30 (latest 3.2.x).
  - vk-bootstrap 1.3.275 (matches the minimum supported Vulkan SDK headers).
  - Vulkan Memory Allocator 3.4.0, glm 1.0.3, doctest 2.4.12.
- Install prefix: cache variable `DEPS_INSTALL_DIR`, default `<repo>/deps/install`; the
  `deps/CMakePresets.json` presets set it to `<repo>/deps/install/<preset>` so libraries built by
  different toolchains (e.g. VS 2022 vs VS 2026) never mix.
- Static builds: wxWidgets (`wxBUILD_SHARED=OFF`), SDL3 (`SDL_STATIC=ON`, `SDL_SHARED=OFF`),
  vk-bootstrap static library.
- Multi-config generators (Visual Studio): dependencies are built and installed for Debug and
  Release, so MSVC debug/release runtimes are never mixed. Single-config generators build Release.
- Every sub-build receives `CMAKE_POLICY_VERSION_MINIMUM=3.5` for compatibility with CMake 4.

### 7.2 Application

- `cmake_minimum_required(VERSION 3.24)`; C++20, no extensions.
- `find_package`: `wxWidgets` (core, base), `SDL3`, `vk-bootstrap`, `VulkanMemoryAllocator`,
  `glm`, `Vulkan COMPONENTS glslc`, `doctest` (tests only).
- Targets:
  - `scene` — static library.
  - `render` — static library (Vulkan renderer).
  - `cubeapp` — GUI executable (`WIN32` on Windows, `MACOSX_BUNDLE` on macOS).
  - `scene_tests` — doctest executable registered with CTest.
- Option `CUBEAPP_BUILD_APP` (default ON) gates `render` and `cubeapp`, so `scene` and its tests
  build with only glm and doctest.
- Shader pipeline: custom commands run `glslc -mfmt=c` → `.spv.inc` in the build tree; rebuilds on
  shader change.
- Warnings: `/W4 /WX /permissive-` (MSVC) or `-Wall -Wextra -Wpedantic -Werror` on project C++
  code (not on resource files). Third-party headers come through imported targets and are
  therefore `SYSTEM` includes, which also covers the VMA implementation in `render/vma_impl.cpp`.
- Windows: `src/app/cubeapp.rc` includes `wx/msw/wx.rc` with the per-monitor-v2 DPI-aware
  manifest (themed common controls); the linker's default manifest is disabled (`/MANIFEST:NO`).
- Linux: GTK3 and X11 linked (GDK X11 used by `embed_x11.cpp`).
- macOS: `embed_cocoa.mm` compiled as Objective-C++; Cocoa and QuartzCore linked. Running requires
  the Vulkan SDK environment (`setup-env.sh`) so the loader finds MoltenVK.

### 7.3 Presets

`CMakePresets.json`, each setting `CMAKE_PREFIX_PATH` to the matching dependency install
(`${sourceDir}/deps/install/vs2022`, `.../vs2026`, `.../ninja`):

- `vs2022` — Visual Studio 17 2022, x64 (primary on Windows), with build presets
  `vs2022-debug` and `vs2022-release`.
- `vs2026` — Visual Studio 18 2026, x64, with matching build presets.
- `ninja-debug`, `ninja-release` — Ninja, for Linux and macOS.

### 7.4 Prerequisites

- CMake 3.24+ (CMake 4.x supported).
- Vulkan SDK 1.3.275 or newer (provides headers, loader, `glslc`, validation layers; MoltenVK on
  macOS).
- Windows: Visual Studio 2022 or 2026 with the C++ desktop workload.
- Linux: C++20 compiler, GTK3 and X11 development packages, Ninja.
- macOS: Xcode command-line tools, Ninja.

No machine-specific paths, user names, or host names appear in committed files.

## 8. Error Handling

- Initialization failures (no Vulkan loader, no suitable GPU, SDL window embedding failure,
  surface creation failure) throw `std::runtime_error` with the specific reason from
  vk-bootstrap or `SDL_GetError()`. `MainFrame` catches, shows a message box, and the application
  exits with a non-zero code.
- `VK_CHECK` converts unexpected `VkResult` values into the same exception path, including
  `VK_ERROR_DEVICE_LOST`.
- `VK_ERROR_OUT_OF_DATE_KHR`, `VK_SUBOPTIMAL_KHR`, and zero-size surfaces are handled silently
  (rebuild or skip).
- Shutdown order in `RenderPanel` close/destroy path: stop frame loop → `Renderer::waitIdle()` →
  destroy renderer (reverse creation order) → `SDL_DestroyWindow` → wxWidgets destroys the panel.

## 9. Testing and Verification

- Unit tests (doctest via CTest), written test-first for `scene/`:
  - `SpinController`: spinning advances orientation; stopped does not; drag rotates directly and
    pauses spin; fast flick sets axis perpendicular to drag; slow release keeps the axis; flick
    while stopped does not start spinning; speed unchanged by flick.
  - `Camera`: zoom clamping at both limits; projection for a given aspect; zero height guarded.
  - `FpsCounter`: exact results with an injected fake clock.
- Verification on Windows (Visual Studio 2022):
  - Clean dependency build (Debug and Release) and clean application build in both
    configurations with zero warnings.
  - All unit tests pass.
  - Manual run (Debug): cube renders; status bar shows FPS and state; Start/Stop toggles; flick
    changes direction; zoom clamps at both limits; resize, maximize, minimize/restore work;
    no validation-layer errors through a clean exit.
- Linux and macOS: implemented per this design but not verified in this environment; the README
  states this explicitly.

## 10. Out of Scope

- Native Wayland embedding.
- MoltenVK bundling and macOS `.app` packaging/signing.
- CI (a GitHub Actions matrix can be added once the repository has a remote).
- Vsync toggle, keyboard controls, textures, lighting, installers.
