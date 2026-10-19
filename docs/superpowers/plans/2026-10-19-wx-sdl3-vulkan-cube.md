# wxWidgets + SDL3 + Vulkan Spinning Cube Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A cross-platform wxWidgets application whose main window embeds an SDL3-wrapped native child window rendering a spinning, colored, mouse-controllable cube with Vulkan.

**Architecture:** wxWidgets owns the app and event loop. `RenderPanel` (a native child `wxWindow`) is wrapped by SDL3 (`SDL_CreateWindowWithProperties` with the native handle) only to obtain a Vulkan surface. Rendering uses plain Vulkan via vk-bootstrap (instance/device/swapchain) and VMA (memory). Pure logic (spin, flick, zoom, FPS, cube geometry) lives in a wx/Vulkan-free `scene` library with doctest unit tests.

**Tech Stack:** C++20, CMake ≥ 3.24 (tested with 4.3), Visual Studio 2022, wxWidgets 3.2.9, SDL 3.2.30, vk-bootstrap 1.3.275, Vulkan Memory Allocator 3.4.0, glm 1.0.3, doctest 2.4.12, Vulkan SDK ≥ 1.3.275 (`glslc`).

Spec: `docs/superpowers/specs/2026-10-19-wx-sdl3-vulkan-cube-design.md`

## Global Constraints

- Windows commands are PowerShell; never use bash/sh on the Windows host.
- No machine-specific values in committed files: no absolute paths, user names, host names, IPs. Paths are relative to the repo or come from presets/cache variables.
- C++20, no compiler extensions. Project code compiles warning-free with `/W4 /WX /permissive-` (MSVC) or `-Wall -Wextra -Wpedantic -Werror`; third-party headers come through imported targets (SYSTEM includes).
- All third-party libraries static. MSVC runtime is the default `/MD` / `/MDd`.
- Dependencies are built by `deps/` into `deps/install/<preset>`; the app finds them only via `find_package` + `CMAKE_PREFIX_PATH`.
- Vulkan API 1.1 baseline, classic `VkRenderPass`, FIFO present mode, 2 frames in flight.
- Spin: initial axis `normalize(1,1,0)`, 90°/s; drag 0.01 rad/px; flick window 100 ms, threshold 50 px/s; flick changes axis only.
- Zoom: ×0.9 per wheel notch toward the cube, distance clamped to [1.8, 10.0], default 3.0. Projection 45° FOV, near 0.1, far 100.
- Status bar field 0: `FPS: <one decimal>` refreshed every 0.5 s; field 1: `Spinning` / `Stopped`. Button label `Stop` while spinning, `Start` while stopped.
- Linux: X11 only (`GDK_BACKEND=x11`, `SDL_VIDEO_DRIVER=x11` unless user-set). macOS: MoltenVK. Mac work happens only after the user approves the Windows build.
- Commit after each task; commit messages end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## File Map

```text
deps/CMakeLists.txt           superbuild (ExternalProject) for all third-party libs
deps/versions.cmake           pinned URLs + SHA-256
deps/CMakePresets.json        vs2022 / vs2026 / ninja dependency builds
CMakeLists.txt                scene lib, tests, render lib, cubeapp
CMakePresets.json             app presets (prefix path = deps/install/<preset>)
README.md                     prerequisites, build, run, platform status
shaders/cube.vert|frag        GLSL, compiled by glslc to C initializer lists
src/scene/                    SpinController, Camera, FpsCounter, CubeGeometry (pure, tested)
src/render/                   Log, VkCheck, VulkanContext, Swapchain, CubeMesh, CubePipeline, Renderer, vma_impl
src/platform/                 embed.h, embed_detail.h, embed_common.cpp, embed_win32.cpp, embed_x11.cpp, embed_cocoa.mm
src/app/                      App, MainFrame, RenderPanel, cubeapp.rc
tests/                        doctest tests for scene/
```

---

### Task 1: Dependency superbuild

**Files:**
- Create: `deps/versions.cmake`, `deps/CMakeLists.txt`, `deps/CMakePresets.json`

**Interfaces:**
- Produces: install prefix `deps/install/vs2022` (Windows) containing CMake packages `glm` (`glm::glm-header-only`), `doctest` (`doctest::doctest`), `VulkanMemoryAllocator` (`GPUOpen::VulkanMemoryAllocator`), `vk-bootstrap` (`vk-bootstrap::vk-bootstrap`), `SDL3` (`SDL3::SDL3`), `wxWidgets` (`wx::core`, `wx::base`), Debug and Release.

- [ ] **Step 1: Write `deps/versions.cmake`**

```cmake
# Pinned third-party sources for the dependency superbuild.

set(WXWIDGETS_VERSION 3.2.9)
set(WXWIDGETS_URL https://github.com/wxWidgets/wxWidgets/releases/download/v${WXWIDGETS_VERSION}/wxWidgets-${WXWIDGETS_VERSION}.tar.bz2)
set(WXWIDGETS_SHA256 fb90f9538bffd6a02edbf80037a0c14c2baf9f509feac8f76ab2a5e4321f112b)

set(SDL3_VERSION 3.2.30)
set(SDL3_URL https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_VERSION}/SDL3-${SDL3_VERSION}.tar.gz)
set(SDL3_SHA256 4c3b09330d866dc52eb65b66259a6684ad387252ca8c57901b3a2b534eb42e3d)

set(VK_BOOTSTRAP_VERSION 1.3.275)
set(VK_BOOTSTRAP_URL https://github.com/charles-lunarg/vk-bootstrap/archive/refs/tags/v${VK_BOOTSTRAP_VERSION}.tar.gz)
set(VK_BOOTSTRAP_SHA256 33737a731e8b3a4539812e63502ea55dffb3dbc7331b44c4cc0902fbf797f5ff)

set(VMA_VERSION 3.4.0)
set(VMA_URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v${VMA_VERSION}.tar.gz)
set(VMA_SHA256 822aa850c6ce77346ae96a8a1d351d52e77e85929f35363849a0a4e638e0a2a1)

set(GLM_VERSION 1.0.3)
set(GLM_URL https://github.com/g-truc/glm/archive/refs/tags/${GLM_VERSION}.tar.gz)
set(GLM_SHA256 6775e47231a446fd086d660ecc18bcd076531cfedd912fbd66e576b118607001)

set(DOCTEST_VERSION 2.4.12)
set(DOCTEST_URL https://github.com/doctest/doctest/archive/refs/tags/v${DOCTEST_VERSION}.tar.gz)
set(DOCTEST_SHA256 73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70)
```

- [ ] **Step 2: Write `deps/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.24)
project(cubeapp_deps LANGUAGES C CXX)

include(ExternalProject)
include(${CMAKE_CURRENT_SOURCE_DIR}/versions.cmake)

set(DEPS_INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH
    "Install prefix for the third-party dependencies")

get_property(DEPS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT DEPS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type for the dependencies" FORCE)
endif()

set(DEPS_COMMON_ARGS
    -DCMAKE_INSTALL_PREFIX=${DEPS_INSTALL_DIR}
    -DCMAKE_PREFIX_PATH=${DEPS_INSTALL_DIR}
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    -DBUILD_SHARED_LIBS=OFF
)
if(NOT DEPS_MULTI_CONFIG)
    list(APPEND DEPS_COMMON_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER})
endif()
if(APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET)
    list(APPEND DEPS_COMMON_ARGS -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET})
endif()

# Sub-builds compile in parallel and follow the configuration being built.
set(DEPS_BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG> --parallel)

ExternalProject_Add(glm
    PREFIX ${CMAKE_BINARY_DIR}/glm
    URL ${GLM_URL}
    URL_HASH SHA256=${GLM_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    CMAKE_ARGS ${DEPS_COMMON_ARGS}
        -DGLM_BUILD_LIBRARY=OFF
        -DGLM_BUILD_TESTS=OFF
        -DGLM_BUILD_INSTALL=ON
    BUILD_COMMAND ${DEPS_BUILD_COMMAND})

ExternalProject_Add(doctest
    PREFIX ${CMAKE_BINARY_DIR}/doctest
    URL ${DOCTEST_URL}
    URL_HASH SHA256=${DOCTEST_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    CMAKE_ARGS ${DEPS_COMMON_ARGS}
        -DDOCTEST_WITH_TESTS=OFF
        -DDOCTEST_WITH_MAIN_IN_STATIC_LIB=OFF
    BUILD_COMMAND ${DEPS_BUILD_COMMAND})

ExternalProject_Add(vma
    PREFIX ${CMAKE_BINARY_DIR}/vma
    URL ${VMA_URL}
    URL_HASH SHA256=${VMA_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    CMAKE_ARGS ${DEPS_COMMON_ARGS}
        -DVMA_ENABLE_INSTALL=ON
        -DVMA_BUILD_SAMPLES=OFF
        -DVMA_BUILD_DOCUMENTATION=OFF
    BUILD_COMMAND ${DEPS_BUILD_COMMAND})

# vk-bootstrap and SDL do not suffix debug libraries themselves; the postfix lets
# Debug and Release share one install prefix.
ExternalProject_Add(vk_bootstrap
    PREFIX ${CMAKE_BINARY_DIR}/vkb
    URL ${VK_BOOTSTRAP_URL}
    URL_HASH SHA256=${VK_BOOTSTRAP_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    CMAKE_ARGS ${DEPS_COMMON_ARGS}
        -DCMAKE_DEBUG_POSTFIX=d
        -DVK_BOOTSTRAP_TEST=OFF
        -DVK_BOOTSTRAP_INSTALL=ON
    BUILD_COMMAND ${DEPS_BUILD_COMMAND})

ExternalProject_Add(sdl3
    PREFIX ${CMAKE_BINARY_DIR}/sdl3
    URL ${SDL3_URL}
    URL_HASH SHA256=${SDL3_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    CMAKE_ARGS ${DEPS_COMMON_ARGS}
        -DCMAKE_DEBUG_POSTFIX=d
        -DSDL_SHARED=OFF
        -DSDL_STATIC=ON
        -DSDL_TEST_LIBRARY=OFF
        -DSDL_TESTS=OFF
        -DSDL_EXAMPLES=OFF
        -DSDL_INSTALL=ON
    BUILD_COMMAND ${DEPS_BUILD_COMMAND})

# wxWidgets adds its own "d" suffix to debug libraries.
ExternalProject_Add(wxwidgets
    PREFIX ${CMAKE_BINARY_DIR}/wx
    URL ${WXWIDGETS_URL}
    URL_HASH SHA256=${WXWIDGETS_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    CMAKE_ARGS ${DEPS_COMMON_ARGS}
        -DwxBUILD_SHARED=OFF
        -DwxBUILD_SAMPLES=OFF
        -DwxBUILD_TESTS=OFF
        -DwxBUILD_DEMOS=OFF
        -DwxBUILD_INSTALL=ON
        -DwxUSE_STC=OFF
        -DwxUSE_WEBVIEW=OFF
        -DwxUSE_MEDIACTRL=OFF
        -DwxUSE_OPENGL=OFF
    BUILD_COMMAND ${DEPS_BUILD_COMMAND})
```

- [ ] **Step 3: Write `deps/CMakePresets.json`**

```json
{
  "version": 5,
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": { "DEPS_INSTALL_DIR": "${sourceDir}/install/${presetName}" }
    },
    {
      "name": "vs2022",
      "inherits": "base",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" }
    },
    {
      "name": "vs2026",
      "inherits": "base",
      "generator": "Visual Studio 18 2026",
      "architecture": "x64",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" }
    },
    {
      "name": "ninja",
      "inherits": "base",
      "generator": "Ninja",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    }
  ],
  "buildPresets": [
    { "name": "vs2022-debug", "configurePreset": "vs2022", "configuration": "Debug" },
    { "name": "vs2022-release", "configurePreset": "vs2022", "configuration": "Release" },
    { "name": "vs2026-debug", "configurePreset": "vs2026", "configuration": "Debug" },
    { "name": "vs2026-release", "configurePreset": "vs2026", "configuration": "Release" },
    { "name": "ninja", "configurePreset": "ninja" }
  ]
}
```

- [ ] **Step 4: Configure and build the fast dependencies first (Debug + Release)**

Run (PowerShell, from repo root):
```powershell
cmake -S deps --preset vs2022
cmake --build deps/build/vs2022 --config Debug --target glm doctest vma vk_bootstrap
cmake --build deps/build/vs2022 --config Release --target glm doctest vma vk_bootstrap
```
Expected: exit code 0; `deps/install/vs2022/share/glm/glmConfig.cmake`, `deps/install/vs2022/lib/cmake/doctest/doctestConfig.cmake`, `deps/install/vs2022/share/cmake/VulkanMemoryAllocator/VulkanMemoryAllocatorConfig.cmake`, `deps/install/vs2022/lib/vk-bootstrap.lib` and `vk-bootstrapd.lib` exist.

- [ ] **Step 5: Build SDL3 and wxWidgets (long; run in background, ~20–40 min)**

```powershell
cmake --build deps/build/vs2022 --config Debug
cmake --build deps/build/vs2022 --config Release
```
Expected: exit 0; `deps/install/vs2022/lib/SDL3-static.lib` and `SDL3-staticd.lib`; `deps/install/vs2022/lib/vc_x64_lib/wxmsw32u_core.lib` and `wxmsw32ud_core.lib`; a `wxWidgetsConfig.cmake` under `deps/install/vs2022/lib/cmake/`.

- [ ] **Step 6: Commit**

```powershell
git add deps/CMakeLists.txt deps/versions.cmake deps/CMakePresets.json
git commit -m "build: add dependency superbuild for wxWidgets, SDL3, vk-bootstrap, VMA, glm, doctest"
```

---

### Task 2: Top-level CMake, `scene` library, unit tests (TDD)

**Files:**
- Create: `CMakeLists.txt`, `CMakePresets.json`
- Create: `src/scene/SpinController.h|.cpp`, `src/scene/Camera.h|.cpp`, `src/scene/FpsCounter.h|.cpp`, `src/scene/CubeGeometry.h|.cpp`
- Test: `tests/test_main.cpp`, `tests/test_spin_controller.cpp`, `tests/test_camera.cpp`, `tests/test_fps_counter.cpp`, `tests/test_cube_geometry.cpp`

**Interfaces:**
- Consumes: Task 1 packages `glm`, `doctest`.
- Produces (namespace `scene`, include root `src/`):
  - `class SpinController` — `void update(float dt)`, `void setSpinning(bool)`, `bool isSpinning() const`, `void beginDrag(glm::vec2 pos, double time)`, `void dragTo(glm::vec2 pos, double time)`, `void endDrag(double time)`, `bool isDragging() const`, `glm::quat orientation() const`, `glm::vec3 axis() const`, `float speed() const`; constants `kDefaultSpeed`, `kDragRadiansPerPixel`, `kFlickMinSpeed`, `kFlickWindow`.
  - `class Camera` — `void zoom(float notches)`, `float distance() const`, `void setViewport(int w, int h)`, `float aspect() const`, `glm::mat4 view() const`, `glm::mat4 projection() const`, `glm::mat4 viewProjection() const`.
  - `class FpsCounter` — `explicit FpsCounter(double interval = 0.5)`, `bool frame(double now)`, `double fps() const`.
  - `struct CubeVertex { glm::vec3 position; glm::vec3 color; }`, `const std::array<CubeVertex, 24>& cubeVertices()`, `const std::array<std::uint16_t, 36>& cubeIndices()`.
  - CMake option `CUBEAPP_BUILD_APP` (default ON); targets `scene`, `scene_tests`; function `cubeapp_set_warnings(target)`.

- [ ] **Step 1: Write `CMakeLists.txt`**

This task writes the file up to and including the tests block, followed by an empty
`if(CUBEAPP_BUILD_APP)` / `endif()` pair. The full content below is the FINAL file; in this task
write only the lines up to `endif()` of the `BUILD_TESTING` block plus:

```cmake
if(CUBEAPP_BUILD_APP)
endif()
```

Task 3 fills the `render` part of that block (from `find_package(Vulkan ...)` through
`cubeapp_set_warnings(render)`, without the wxWidgets `find_package`), and Task 4 adds the
wxWidgets `find_package` and everything from `if(WIN32)` to `set_property(... VS_STARTUP_PROJECT ...)`.
Final file:

```cmake
cmake_minimum_required(VERSION 3.24)
project(cubeapp
    VERSION 0.1.0
    DESCRIPTION "wxWidgets + SDL3 + Vulkan spinning cube"
    LANGUAGES C CXX
)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CUBEAPP_BUILD_APP "Build the GUI application (needs wxWidgets, SDL3 and Vulkan)" ON)
include(CTest)

function(cubeapp_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive->")
    else()
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wall;-Wextra;-Wpedantic;-Werror>")
    endif()
endfunction()

find_package(glm CONFIG REQUIRED)

add_library(scene STATIC
    src/scene/Camera.cpp
    src/scene/CubeGeometry.cpp
    src/scene/FpsCounter.cpp
    src/scene/SpinController.cpp
)
target_include_directories(scene PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(scene PUBLIC glm::glm-header-only)
cubeapp_set_warnings(scene)

if(BUILD_TESTING)
    find_package(doctest CONFIG REQUIRED)
    add_executable(scene_tests
        tests/test_main.cpp
        tests/test_camera.cpp
        tests/test_cube_geometry.cpp
        tests/test_fps_counter.cpp
        tests/test_spin_controller.cpp
    )
    target_link_libraries(scene_tests PRIVATE scene doctest::doctest)
    cubeapp_set_warnings(scene_tests)
    add_test(NAME scene_tests COMMAND scene_tests)
endif()

if(CUBEAPP_BUILD_APP)
    find_package(Vulkan REQUIRED COMPONENTS glslc)
    find_package(SDL3 CONFIG REQUIRED)
    find_package(vk-bootstrap CONFIG REQUIRED)
    find_package(VulkanMemoryAllocator CONFIG REQUIRED)
    find_package(wxWidgets 3.2 CONFIG REQUIRED COMPONENTS core base)

    # GLSL -> SPIR-V emitted as C initializer lists, #included by CubePipeline.cpp.
    set(CUBEAPP_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
    set(CUBEAPP_SHADER_OUTPUTS)
    foreach(shader cube.vert cube.frag)
        set(output ${CUBEAPP_SHADER_DIR}/${shader}.spv.inc)
        add_custom_command(
            OUTPUT ${output}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CUBEAPP_SHADER_DIR}
            COMMAND Vulkan::glslc --target-env=vulkan1.1 -mfmt=c -o ${output}
                    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}
            COMMENT "Compiling shader ${shader}"
            VERBATIM)
        list(APPEND CUBEAPP_SHADER_OUTPUTS ${output})
    endforeach()

    add_library(render STATIC
        src/render/CubeMesh.cpp
        src/render/CubePipeline.cpp
        src/render/Log.cpp
        src/render/Renderer.cpp
        src/render/Swapchain.cpp
        src/render/VkCheck.cpp
        src/render/VulkanContext.cpp
        src/render/vma_impl.cpp
        ${CUBEAPP_SHADER_OUTPUTS}
    )
    target_include_directories(render PRIVATE ${CUBEAPP_SHADER_DIR})
    target_link_libraries(render PUBLIC
        scene
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
        vk-bootstrap::vk-bootstrap
        SDL3::SDL3
    )
    cubeapp_set_warnings(render)

    if(WIN32)
        set(CUBEAPP_PLATFORM_SOURCES src/platform/embed_win32.cpp src/app/cubeapp.rc)
    elseif(APPLE)
        set(CUBEAPP_PLATFORM_SOURCES src/platform/embed_cocoa.mm)
    else()
        set(CUBEAPP_PLATFORM_SOURCES src/platform/embed_x11.cpp)
    endif()

    add_executable(cubeapp WIN32 MACOSX_BUNDLE
        src/app/App.cpp
        src/app/MainFrame.cpp
        src/app/RenderPanel.cpp
        src/platform/embed_common.cpp
        ${CUBEAPP_PLATFORM_SOURCES}
    )
    target_link_libraries(cubeapp PRIVATE render wx::core wx::base)
    cubeapp_set_warnings(cubeapp)

    if(MSVC)
        # The application manifest comes from wx/msw/wx.rc (cubeapp.rc).
        target_link_options(cubeapp PRIVATE /MANIFEST:NO)
    endif()
    if(APPLE)
        target_link_libraries(cubeapp PRIVATE "-framework Cocoa" "-framework QuartzCore")
    elseif(UNIX)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
        find_package(X11 REQUIRED)
        target_link_libraries(cubeapp PRIVATE PkgConfig::GTK3 X11::X11)
    endif()

    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT cubeapp)
endif()
```

- [ ] **Step 2: Write `CMakePresets.json`**

```json
{
  "version": 5,
  "configurePresets": [
    { "name": "base", "hidden": true, "binaryDir": "${sourceDir}/build/${presetName}" },
    {
      "name": "vs2022",
      "inherits": "base",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "cacheVariables": { "CMAKE_PREFIX_PATH": "${sourceDir}/deps/install/vs2022" },
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" }
    },
    {
      "name": "vs2026",
      "inherits": "base",
      "generator": "Visual Studio 18 2026",
      "architecture": "x64",
      "cacheVariables": { "CMAKE_PREFIX_PATH": "${sourceDir}/deps/install/vs2026" },
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" }
    },
    {
      "name": "ninja-debug",
      "inherits": "base",
      "generator": "Ninja",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug", "CMAKE_PREFIX_PATH": "${sourceDir}/deps/install/ninja" }
    },
    {
      "name": "ninja-release",
      "inherits": "base",
      "generator": "Ninja",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release", "CMAKE_PREFIX_PATH": "${sourceDir}/deps/install/ninja" }
    }
  ],
  "buildPresets": [
    { "name": "vs2022-debug", "configurePreset": "vs2022", "configuration": "Debug" },
    { "name": "vs2022-release", "configurePreset": "vs2022", "configuration": "Release" },
    { "name": "vs2026-debug", "configurePreset": "vs2026", "configuration": "Debug" },
    { "name": "vs2026-release", "configurePreset": "vs2026", "configuration": "Release" },
    { "name": "ninja-debug", "configurePreset": "ninja-debug" },
    { "name": "ninja-release", "configurePreset": "ninja-release" }
  ],
  "testPresets": [
    { "name": "vs2022-debug", "configurePreset": "vs2022", "configuration": "Debug", "output": { "outputOnFailure": true } },
    { "name": "vs2022-release", "configurePreset": "vs2022", "configuration": "Release", "output": { "outputOnFailure": true } },
    { "name": "vs2026-debug", "configurePreset": "vs2026", "configuration": "Debug", "output": { "outputOnFailure": true } },
    { "name": "vs2026-release", "configurePreset": "vs2026", "configuration": "Release", "output": { "outputOnFailure": true } },
    { "name": "ninja-debug", "configurePreset": "ninja-debug", "output": { "outputOnFailure": true } },
    { "name": "ninja-release", "configurePreset": "ninja-release", "output": { "outputOnFailure": true } }
  ]
}
```

- [ ] **Step 3: Write the scene headers (interfaces only) so tests compile**

`src/scene/SpinController.h`:
```cpp
#pragma once

#include <deque>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace scene {

// Cube orientation with automatic spin and drag-and-flick control.
// Positions are window pixels (y grows downward); times are seconds.
class SpinController {
public:
    static constexpr float kDefaultSpeed = 1.5707963f; // 90 degrees per second
    static constexpr float kDragRadiansPerPixel = 0.01f;
    static constexpr float kFlickMinSpeed = 50.0f; // pixels per second
    static constexpr double kFlickWindow = 0.1;    // seconds

    SpinController();

    void update(float dt);

    void setSpinning(bool spinning) { spinning_ = spinning; }
    bool isSpinning() const { return spinning_; }

    void beginDrag(glm::vec2 pos, double time);
    void dragTo(glm::vec2 pos, double time);
    void endDrag(double time);
    bool isDragging() const { return dragging_; }

    glm::quat orientation() const { return orientation_; }
    glm::vec3 axis() const { return axis_; }
    float speed() const { return speed_; }

private:
    struct Sample {
        glm::vec2 pos;
        double time;
    };

    static glm::vec3 screenAxis(glm::vec2 direction);
    void dropSamplesBefore(double time);

    glm::quat orientation_{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 axis_;
    float speed_ = kDefaultSpeed;
    bool spinning_ = true;
    bool dragging_ = false;
    std::deque<Sample> samples_;
};

} // namespace scene
```

`src/scene/Camera.h`:
```cpp
#pragma once

#include <glm/glm.hpp>

namespace scene {

// Camera on the +Z axis looking at the origin, with clamped zoom.
class Camera {
public:
    static constexpr float kMinDistance = 1.8f;
    static constexpr float kMaxDistance = 10.0f;
    static constexpr float kDefaultDistance = 3.0f;
    static constexpr float kZoomFactorPerNotch = 0.9f;
    static constexpr float kFovYDegrees = 45.0f;
    static constexpr float kNear = 0.1f;
    static constexpr float kFar = 100.0f;

    // Positive notches move the camera closer; fractional notches are allowed.
    void zoom(float notches);
    float distance() const { return distance_; }

    void setViewport(int width, int height);
    float aspect() const { return aspect_; }

    glm::mat4 view() const;
    // Vulkan clip space: depth in [0, 1], world +Y up on screen.
    glm::mat4 projection() const;
    glm::mat4 viewProjection() const { return projection() * view(); }

private:
    float distance_ = kDefaultDistance;
    float aspect_ = 1.0f;
};

} // namespace scene
```

`src/scene/FpsCounter.h`:
```cpp
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
```

`src/scene/CubeGeometry.h`:
```cpp
#pragma once

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

namespace scene {

struct CubeVertex {
    glm::vec3 position;
    glm::vec3 color;
};

// Unit cube centred at the origin (half-extent 0.5), one solid colour per face,
// counter-clockwise winding when viewed from outside.
const std::array<CubeVertex, 24>& cubeVertices();
const std::array<std::uint16_t, 36>& cubeIndices();

} // namespace scene
```

- [ ] **Step 4: Write the failing tests**

`tests/test_main.cpp`:
```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
```

`tests/test_spin_controller.cpp`:
```cpp
#include <cmath>

#include <doctest/doctest.h>
#include <glm/gtc/quaternion.hpp>

#include "scene/SpinController.h"

using scene::SpinController;

namespace {

bool near(glm::vec3 a, glm::vec3 b, float eps = 1e-4f)
{
    return glm::length(a - b) < eps;
}

glm::vec3 front(const SpinController& spin)
{
    return spin.orientation() * glm::vec3(0.0f, 0.0f, 1.0f);
}

// Five samples 20 ms apart, each moving by `step` pixels, released at t = 0.1 s.
void flick(SpinController& spin, glm::vec2 step)
{
    spin.beginDrag({0.0f, 0.0f}, 0.0);
    for (int i = 1; i <= 5; ++i) {
        spin.dragTo(step * static_cast<float>(i), 0.02 * i);
    }
    spin.endDrag(0.1);
}

const glm::vec3 kDefaultAxis = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));

} // namespace

TEST_CASE("starts spinning about normalize(1,1,0) at 90 degrees per second")
{
    SpinController spin;
    CHECK(spin.isSpinning());
    CHECK_FALSE(spin.isDragging());
    CHECK(near(spin.axis(), kDefaultAxis));
    CHECK(spin.speed() == doctest::Approx(1.5707963f));
}

TEST_CASE("update while spinning rotates by speed * dt about the axis")
{
    SpinController spin;
    spin.update(0.5f);
    const glm::quat expected = glm::angleAxis(spin.speed() * 0.5f, kDefaultAxis);
    CHECK(near(front(spin), expected * glm::vec3(0.0f, 0.0f, 1.0f)));
}

TEST_CASE("update while stopped leaves the orientation unchanged")
{
    SpinController spin;
    spin.setSpinning(false);
    spin.update(1.0f);
    CHECK(near(front(spin), {0.0f, 0.0f, 1.0f}));
}

TEST_CASE("dragging right rotates about +Y by 0.01 rad per pixel")
{
    SpinController spin;
    spin.setSpinning(false);
    spin.beginDrag({0.0f, 0.0f}, 0.0);
    spin.dragTo({100.0f, 0.0f}, 0.5);
    CHECK(near(front(spin), {std::sin(1.0f), 0.0f, std::cos(1.0f)}));
}

TEST_CASE("dragging down moves the front face down")
{
    SpinController spin;
    spin.setSpinning(false);
    spin.beginDrag({0.0f, 0.0f}, 0.0);
    spin.dragTo({0.0f, 100.0f}, 0.5);
    CHECK(near(front(spin), {0.0f, -std::sin(1.0f), std::cos(1.0f)}));
}

TEST_CASE("automatic spin pauses while dragging but stays enabled")
{
    SpinController spin;
    spin.beginDrag({0.0f, 0.0f}, 0.0);
    spin.update(1.0f);
    CHECK(near(front(spin), {0.0f, 0.0f, 1.0f}));
    CHECK(spin.isSpinning());
    CHECK(spin.isDragging());
}

TEST_CASE("fast horizontal flick sets the spin axis to +Y")
{
    SpinController spin;
    flick(spin, {10.0f, 0.0f});
    CHECK_FALSE(spin.isDragging());
    CHECK(near(spin.axis(), {0.0f, 1.0f, 0.0f}));
}

TEST_CASE("fast downward flick sets the spin axis to +X")
{
    SpinController spin;
    flick(spin, {0.0f, 10.0f});
    CHECK(near(spin.axis(), {1.0f, 0.0f, 0.0f}));
}

TEST_CASE("flick to the left reverses the direction")
{
    SpinController spin;
    flick(spin, {-10.0f, 0.0f});
    CHECK(near(spin.axis(), {0.0f, -1.0f, 0.0f}));
}

TEST_CASE("slow release keeps the previous axis")
{
    SpinController spin;
    spin.beginDrag({0.0f, 0.0f}, 0.0);
    spin.dragTo({2.0f, 0.0f}, 0.05);
    spin.endDrag(0.1);
    CHECK(near(spin.axis(), kDefaultAxis));
}

TEST_CASE("holding still before release keeps the previous axis")
{
    SpinController spin;
    spin.beginDrag({0.0f, 0.0f}, 0.0);
    spin.dragTo({50.0f, 0.0f}, 0.05);
    spin.endDrag(1.0);
    CHECK(near(spin.axis(), kDefaultAxis));
}

TEST_CASE("click without movement keeps the previous axis")
{
    SpinController spin;
    spin.beginDrag({5.0f, 5.0f}, 0.0);
    spin.endDrag(0.05);
    CHECK(near(spin.axis(), kDefaultAxis));
}

TEST_CASE("flick keeps the speed and the stopped state")
{
    SpinController spin;
    spin.setSpinning(false);
    flick(spin, {10.0f, 0.0f});
    CHECK_FALSE(spin.isSpinning());
    CHECK(spin.speed() == doctest::Approx(SpinController::kDefaultSpeed));
    CHECK(near(spin.axis(), {0.0f, 1.0f, 0.0f}));
}
```

`tests/test_camera.cpp`:
```cpp
#include <cmath>

#include <doctest/doctest.h>

#include "scene/Camera.h"

using scene::Camera;

TEST_CASE("default distance is 3")
{
    Camera camera;
    CHECK(camera.distance() == doctest::Approx(3.0f));
}

TEST_CASE("one notch in multiplies the distance by 0.9")
{
    Camera camera;
    camera.zoom(1.0f);
    CHECK(camera.distance() == doctest::Approx(2.7f));
}

TEST_CASE("one notch out divides the distance by 0.9")
{
    Camera camera;
    camera.zoom(-1.0f);
    CHECK(camera.distance() == doctest::Approx(3.0f / 0.9f));
}

TEST_CASE("fractional notches scale the exponent")
{
    Camera camera;
    camera.zoom(0.5f);
    CHECK(camera.distance() == doctest::Approx(3.0f * std::pow(0.9f, 0.5f)));
}

TEST_CASE("zoom clamps at both limits")
{
    Camera camera;
    camera.zoom(100.0f);
    CHECK(camera.distance() == doctest::Approx(Camera::kMinDistance));
    camera.zoom(-1000.0f);
    CHECK(camera.distance() == doctest::Approx(Camera::kMaxDistance));
}

TEST_CASE("aspect follows the viewport and guards zero height")
{
    Camera camera;
    camera.setViewport(800, 400);
    CHECK(camera.aspect() == doctest::Approx(2.0f));
    camera.setViewport(800, 0);
    CHECK(camera.aspect() == doctest::Approx(1.0f));
}

TEST_CASE("projection uses Vulkan conventions")
{
    Camera camera;
    camera.setViewport(800, 400);
    const glm::mat4 p = camera.projection();
    CHECK(p[1][1] < 0.0f);
    CHECK(p[0][0] == doctest::Approx(-p[1][1] / 2.0f));

    const glm::vec4 centre = camera.viewProjection() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    CHECK(centre.z / centre.w > 0.0f);
    CHECK(centre.z / centre.w < 1.0f);

    // World up maps to negative clip-space Y (top of the framebuffer in Vulkan).
    const glm::vec4 top = camera.viewProjection() * glm::vec4(0.0f, 0.5f, 0.0f, 1.0f);
    CHECK(top.y / top.w < 0.0f);
}
```

`tests/test_fps_counter.cpp`:
```cpp
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
```

`tests/test_cube_geometry.cpp`:
```cpp
#include <cmath>
#include <set>
#include <tuple>

#include <doctest/doctest.h>

#include "scene/CubeGeometry.h"

using scene::cubeIndices;
using scene::cubeVertices;

TEST_CASE("indices reference existing vertices")
{
    for (auto index : cubeIndices()) {
        CHECK(index < cubeVertices().size());
    }
}

TEST_CASE("all vertices lie on the unit cube corners")
{
    for (const auto& v : cubeVertices()) {
        CHECK(std::abs(v.position.x) == doctest::Approx(0.5f));
        CHECK(std::abs(v.position.y) == doctest::Approx(0.5f));
        CHECK(std::abs(v.position.z) == doctest::Approx(0.5f));
    }
}

TEST_CASE("every triangle is counter-clockwise seen from outside")
{
    const auto& v = cubeVertices();
    const auto& idx = cubeIndices();
    for (std::size_t t = 0; t < idx.size(); t += 3) {
        const glm::vec3 a = v[idx[t]].position;
        const glm::vec3 b = v[idx[t + 1]].position;
        const glm::vec3 c = v[idx[t + 2]].position;
        const glm::vec3 normal = glm::cross(b - a, c - a);
        const glm::vec3 centroid = (a + b + c) / 3.0f;
        CHECK(glm::dot(normal, centroid) > 0.0f);
    }
}

TEST_CASE("each face has one colour and all six colours differ")
{
    const auto& v = cubeVertices();
    std::set<std::tuple<float, float, float>> colours;
    for (std::size_t face = 0; face < 6; ++face) {
        const glm::vec3 c = v[face * 4].color;
        for (std::size_t k = 1; k < 4; ++k) {
            CHECK(v[face * 4 + k].color == c);
        }
        colours.insert({c.r, c.g, c.b});
    }
    CHECK(colours.size() == 6);
}
```

- [ ] **Step 5: Add stub `.cpp` files so the project links, then run tests to see them fail**

Create `src/scene/SpinController.cpp`, `Camera.cpp`, `FpsCounter.cpp`, `CubeGeometry.cpp` each containing only `#include "scene/<Name>.h"`.

Run:
```powershell
cmake --preset vs2022 -DCUBEAPP_BUILD_APP=OFF
cmake --build --preset vs2022-debug --target scene_tests
```
Expected: link FAILS with unresolved externals (`SpinController::SpinController`, `Camera::zoom`, `cubeVertices`, …). This is the red state.

- [ ] **Step 6: Implement `src/scene/SpinController.cpp`**

```cpp
#include "scene/SpinController.h"

namespace scene {

SpinController::SpinController()
    : axis_(glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f)))
{
}

void SpinController::update(float dt)
{
    if (!spinning_ || dragging_) {
        return;
    }
    orientation_ = glm::normalize(glm::angleAxis(speed_ * dt, axis_) * orientation_);
}

void SpinController::beginDrag(glm::vec2 pos, double time)
{
    dragging_ = true;
    samples_.clear();
    samples_.push_back({pos, time});
}

void SpinController::dragTo(glm::vec2 pos, double time)
{
    if (!dragging_) {
        return;
    }
    const glm::vec2 delta = pos - samples_.back().pos;
    const float distance = glm::length(delta);
    if (distance > 0.0f) {
        const glm::quat step = glm::angleAxis(distance * kDragRadiansPerPixel, screenAxis(delta));
        orientation_ = glm::normalize(step * orientation_);
    }
    samples_.push_back({pos, time});
    dropSamplesBefore(time - kFlickWindow);
}

void SpinController::endDrag(double time)
{
    if (!dragging_) {
        return;
    }
    dragging_ = false;
    dropSamplesBefore(time - kFlickWindow);
    if (samples_.size() >= 2) {
        const double span = time - samples_.front().time;
        if (span > 0.0) {
            const glm::vec2 velocity =
                (samples_.back().pos - samples_.front().pos) / static_cast<float>(span);
            if (glm::length(velocity) > kFlickMinSpeed) {
                axis_ = screenAxis(velocity);
            }
        }
    }
    samples_.clear();
}

// A drag along screen direction (dx, dy) turns the cube about world axis (dy, dx, 0):
// right -> +Y, down -> +X (screen y grows downward).
glm::vec3 SpinController::screenAxis(glm::vec2 direction)
{
    return glm::normalize(glm::vec3(direction.y, direction.x, 0.0f));
}

void SpinController::dropSamplesBefore(double time)
{
    while (!samples_.empty() && samples_.front().time < time) {
        samples_.pop_front();
    }
}

} // namespace scene
```

- [ ] **Step 7: Implement `src/scene/Camera.cpp`**

```cpp
#include "scene/Camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace scene {

void Camera::zoom(float notches)
{
    distance_ = std::clamp(distance_ * std::pow(kZoomFactorPerNotch, notches), kMinDistance, kMaxDistance);
}

void Camera::setViewport(int width, int height)
{
    aspect_ = (width > 0 && height > 0) ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
}

glm::mat4 Camera::view() const
{
    return glm::lookAt(glm::vec3(0.0f, 0.0f, distance_), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::projection() const
{
    glm::mat4 p = glm::perspectiveRH_ZO(glm::radians(kFovYDegrees), aspect_, kNear, kFar);
    p[1][1] *= -1.0f; // Vulkan framebuffer Y points down
    return p;
}

} // namespace scene
```

- [ ] **Step 8: Implement `src/scene/FpsCounter.cpp`**

```cpp
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
```

- [ ] **Step 9: Implement `src/scene/CubeGeometry.cpp`**

```cpp
#include "scene/CubeGeometry.h"

namespace scene {

namespace {

constexpr glm::vec3 kRed{1.0f, 0.0f, 0.0f};
constexpr glm::vec3 kGreen{0.0f, 1.0f, 0.0f};
constexpr glm::vec3 kBlue{0.0f, 0.0f, 1.0f};
constexpr glm::vec3 kYellow{1.0f, 1.0f, 0.0f};
constexpr glm::vec3 kCyan{0.0f, 1.0f, 1.0f};
constexpr glm::vec3 kMagenta{1.0f, 0.0f, 1.0f};

constexpr float h = 0.5f;

// Per face: bottom-left, bottom-right, top-right, top-left as seen from outside.
const std::array<CubeVertex, 24> kVertices{{
    // +X
    {{h, -h, h}, kRed}, {{h, -h, -h}, kRed}, {{h, h, -h}, kRed}, {{h, h, h}, kRed},
    // -X
    {{-h, -h, -h}, kGreen}, {{-h, -h, h}, kGreen}, {{-h, h, h}, kGreen}, {{-h, h, -h}, kGreen},
    // +Y
    {{-h, h, h}, kBlue}, {{h, h, h}, kBlue}, {{h, h, -h}, kBlue}, {{-h, h, -h}, kBlue},
    // -Y
    {{-h, -h, -h}, kYellow}, {{h, -h, -h}, kYellow}, {{h, -h, h}, kYellow}, {{-h, -h, h}, kYellow},
    // +Z
    {{-h, -h, h}, kCyan}, {{h, -h, h}, kCyan}, {{h, h, h}, kCyan}, {{-h, h, h}, kCyan},
    // -Z
    {{h, -h, -h}, kMagenta}, {{-h, -h, -h}, kMagenta}, {{-h, h, -h}, kMagenta}, {{h, h, -h}, kMagenta},
}};

std::array<std::uint16_t, 36> makeIndices()
{
    std::array<std::uint16_t, 36> indices{};
    for (std::uint16_t face = 0; face < 6; ++face) {
        const auto base = static_cast<std::uint16_t>(face * 4);
        const std::uint16_t quad[6] = {0, 1, 2, 2, 3, 0};
        for (std::size_t k = 0; k < 6; ++k) {
            indices[face * 6 + k] = static_cast<std::uint16_t>(base + quad[k]);
        }
    }
    return indices;
}

const std::array<std::uint16_t, 36> kIndices = makeIndices();

} // namespace

const std::array<CubeVertex, 24>& cubeVertices()
{
    return kVertices;
}

const std::array<std::uint16_t, 36>& cubeIndices()
{
    return kIndices;
}

} // namespace scene
```

- [ ] **Step 10: Build and run the tests (green)**

```powershell
cmake --build --preset vs2022-debug --target scene_tests
ctest --preset vs2022-debug
```
Expected: build succeeds with no warnings; `100% tests passed, 0 tests failed out of 1`. Also run `build\vs2022\Debug\scene_tests.exe` and confirm doctest reports all test cases passed (status: SUCCESS).

- [ ] **Step 11: Commit**

```powershell
git add CMakeLists.txt CMakePresets.json src/scene tests
git commit -m "feat: add scene library (spin, flick, zoom, fps, cube geometry) with doctest tests"
```

---

### Task 3: Shaders and Vulkan `render` library

**Files:**
- Create: `shaders/cube.vert`, `shaders/cube.frag`
- Create: `src/render/Log.h|.cpp`, `src/render/VkCheck.h|.cpp`, `src/render/VulkanContext.h|.cpp`, `src/render/Swapchain.h|.cpp`, `src/render/CubeMesh.h|.cpp`, `src/render/CubePipeline.h|.cpp`, `src/render/Renderer.h|.cpp`, `src/render/vma_impl.cpp`

**Interfaces:**
- Consumes: `scene::cubeVertices()`, `scene::cubeIndices()`, `scene::CubeVertex`; Task 1 packages (all except wxWidgets are needed to compile; CMake configure with `CUBEAPP_BUILD_APP=ON` needs wxWidgets installed).
- Produces (namespace `render`):
  - `class Renderer` — `Renderer(SDL_Window* window, uint32_t width, uint32_t height, bool enableValidation)`, `void resize(uint32_t width, uint32_t height)`, `void draw(const glm::mat4& mvp)`, `void waitIdle()`. Throws `std::runtime_error` on failure.
  - `void log(const char* message)`.

- [ ] **Step 1: Write shaders**

`shaders/cube.vert`:
```glsl
#version 450

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 vColor;

void main()
{
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    vColor = inColor;
}
```

`shaders/cube.frag`:
```glsl
#version 450

layout(location = 0) in vec3 vColor;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(vColor, 1.0);
}
```

- [ ] **Step 2: Write logging and error helpers**

`src/render/Log.h`:
```cpp
#pragma once

namespace render {

// Writes a diagnostic line to stderr (and the debugger output on Windows).
void log(const char* message);

} // namespace render
```

`src/render/Log.cpp`:
```cpp
#include "render/Log.h"

#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
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
```

`src/render/VkCheck.h`:
```cpp
#pragma once

#include <vulkan/vulkan.h>

namespace render {

const char* vkResultName(VkResult result);
[[noreturn]] void throwVkError(VkResult result, const char* call);

} // namespace render

#define VK_CHECK(call)                                          \
    do {                                                        \
        const VkResult vkCheckResult_ = (call);                 \
        if (vkCheckResult_ != VK_SUCCESS) {                     \
            ::render::throwVkError(vkCheckResult_, #call);      \
        }                                                       \
    } while (false)
```

`src/render/VkCheck.cpp`:
```cpp
#include "render/VkCheck.h"

#include <stdexcept>
#include <string>

namespace render {

const char* vkResultName(VkResult result)
{
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    default: return "unknown VkResult";
    }
}

void throwVkError(VkResult result, const char* call)
{
    throw std::runtime_error(std::string("Vulkan call failed: ") + call + " returned " +
                             vkResultName(result) + " (" + std::to_string(static_cast<int>(result)) + ")");
}

} // namespace render
```

`src/render/vma_impl.cpp`:
```cpp
// The one translation unit that compiles the Vulkan Memory Allocator implementation.
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
```

- [ ] **Step 3: Write `VulkanContext`**

`src/render/VulkanContext.h`:
```cpp
#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

struct SDL_Window;

namespace render {

// Instance, surface, device, queues and allocator for one SDL window.
class VulkanContext {
public:
    VulkanContext(SDL_Window* window, bool enableValidation);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    VkDevice device() const { return device_.device; }
    VkPhysicalDevice physicalDevice() const { return device_.physical_device.physical_device; }
    VkSurfaceKHR surface() const { return surface_; }
    const vkb::Device& vkbDevice() const { return device_; }
    VkQueue graphicsQueue() const { return graphicsQueue_; }
    std::uint32_t graphicsQueueFamily() const { return graphicsQueueFamily_; }
    VkQueue presentQueue() const { return presentQueue_; }
    VmaAllocator allocator() const { return allocator_; }

private:
    void cleanup() noexcept;

    vkb::Instance instance_;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    vkb::Device device_;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    std::uint32_t graphicsQueueFamily_ = 0;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
};

} // namespace render
```

`src/render/VulkanContext.cpp`:
```cpp
#include "render/VulkanContext.h"

#include <stdexcept>
#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_vulkan.h>

#include "render/Log.h"
#include "render/VkCheck.h"

namespace render {

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                             VkDebugUtilsMessageTypeFlagsEXT,
                                             const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        log((std::string("[vulkan] ") + data->pMessage).c_str());
    }
    return VK_FALSE;
}

[[noreturn]] void fail(const std::string& what, const std::string& reason)
{
    throw std::runtime_error(what + ": " + reason);
}

} // namespace

VulkanContext::VulkanContext(SDL_Window* window, bool enableValidation)
{
    try {
        Uint32 extensionCount = 0;
        const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
        if (!extensions) {
            fail("SDL_Vulkan_GetInstanceExtensions failed", SDL_GetError());
        }

        vkb::InstanceBuilder builder;
        builder.set_app_name("cubeapp").require_api_version(1, 1, 0).enable_extensions(extensionCount, extensions);
        if (enableValidation) {
            auto system = vkb::SystemInfo::get_system_info();
            if (system && system->validation_layers_available) {
                builder.request_validation_layers(true).set_debug_callback(debugCallback);
            } else {
                log("Vulkan validation layer not available; continuing without it");
            }
        }
        auto instance = builder.build();
        if (!instance) {
            fail("Vulkan instance creation failed", instance.error().message());
        }
        instance_ = instance.value();

        if (!SDL_Vulkan_CreateSurface(window, instance_.instance, nullptr, &surface_)) {
            fail("Vulkan surface creation failed", SDL_GetError());
        }

        vkb::PhysicalDeviceSelector selector{instance_};
        auto physical = selector.set_surface(surface_)
                            .set_minimum_version(1, 1)
                            .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                            .select();
        if (!physical) {
            fail("No suitable GPU", physical.error().message());
        }

        auto device = vkb::DeviceBuilder{physical.value()}.build();
        if (!device) {
            fail("Vulkan device creation failed", device.error().message());
        }
        device_ = device.value();

        auto graphicsQueue = device_.get_queue(vkb::QueueType::graphics);
        auto graphicsFamily = device_.get_queue_index(vkb::QueueType::graphics);
        auto presentQueue = device_.get_queue(vkb::QueueType::present);
        if (!graphicsQueue || !graphicsFamily || !presentQueue) {
            fail("Vulkan queue lookup failed", "no graphics or present queue");
        }
        graphicsQueue_ = graphicsQueue.value();
        graphicsQueueFamily_ = graphicsFamily.value();
        presentQueue_ = presentQueue.value();

        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
        allocatorInfo.instance = instance_.instance;
        allocatorInfo.physicalDevice = physicalDevice();
        allocatorInfo.device = device_.device;
        VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator_));
    } catch (...) {
        cleanup();
        throw;
    }
}

VulkanContext::~VulkanContext()
{
    cleanup();
}

void VulkanContext::cleanup() noexcept
{
    if (allocator_ != VK_NULL_HANDLE) {
        vmaDestroyAllocator(allocator_);
        allocator_ = VK_NULL_HANDLE;
    }
    if (device_.device != VK_NULL_HANDLE) {
        vkb::destroy_device(device_);
        device_ = vkb::Device{};
    }
    if (surface_ != VK_NULL_HANDLE) {
        vkb::destroy_surface(instance_, surface_);
        surface_ = VK_NULL_HANDLE;
    }
    if (instance_.instance != VK_NULL_HANDLE) {
        vkb::destroy_instance(instance_);
        instance_ = vkb::Instance{};
    }
}

} // namespace render
```

- [ ] **Step 4: Write `Swapchain`**

`src/render/Swapchain.h`:
```cpp
#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>

namespace render {

class VulkanContext;

// Swapchain plus everything sized to it: image views, depth buffer, framebuffers.
// The render pass is created once and survives recreation.
class Swapchain {
public:
    Swapchain(VulkanContext& context, std::uint32_t width, std::uint32_t height);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // The caller must make sure the device is idle.
    void recreate(std::uint32_t width, std::uint32_t height);

    VkSwapchainKHR handle() const { return swapchain_.swapchain; }
    VkExtent2D extent() const { return swapchain_.extent; }
    std::uint32_t imageCount() const { return static_cast<std::uint32_t>(framebuffers_.size()); }
    VkRenderPass renderPass() const { return renderPass_; }
    VkFramebuffer framebuffer(std::uint32_t index) const { return framebuffers_[index]; }

private:
    void createSwapchain(std::uint32_t width, std::uint32_t height);
    void createRenderPass();
    void createSizedResources();
    void destroySizedResources() noexcept;
    void destroyAll() noexcept;

    VulkanContext& context_;
    vkb::Swapchain swapchain_;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkImageView> imageViews_;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VmaAllocation depthAllocation_ = VK_NULL_HANDLE;
    VkImageView depthView_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
};

} // namespace render
```

`src/render/Swapchain.cpp`:
```cpp
#include "render/Swapchain.h"

#include <stdexcept>

#include "render/VkCheck.h"
#include "render/VulkanContext.h"

namespace render {

namespace {

VkFormat chooseDepthFormat(VkPhysicalDevice gpu)
{
    for (VkFormat format : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT,
                            VK_FORMAT_D16_UNORM}) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(gpu, format, &properties);
        if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }
    throw std::runtime_error("No supported depth buffer format");
}

VkImageAspectFlags depthAspect(VkFormat format)
{
    const bool hasStencil = format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    return VK_IMAGE_ASPECT_DEPTH_BIT | (hasStencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
}

} // namespace

Swapchain::Swapchain(VulkanContext& context, std::uint32_t width, std::uint32_t height)
    : context_(context)
{
    try {
        depthFormat_ = chooseDepthFormat(context_.physicalDevice());
        createSwapchain(width, height);
        createRenderPass();
        createSizedResources();
    } catch (...) {
        destroyAll();
        throw;
    }
}

Swapchain::~Swapchain()
{
    destroyAll();
}

void Swapchain::recreate(std::uint32_t width, std::uint32_t height)
{
    const VkFormat previousFormat = swapchain_.image_format;
    destroySizedResources();
    createSwapchain(width, height);
    // The render pass and the pipeline were built for the original format.
    if (swapchain_.image_format != previousFormat) {
        throw std::runtime_error("Swapchain image format changed during recreation");
    }
    createSizedResources();
}

void Swapchain::createSwapchain(std::uint32_t width, std::uint32_t height)
{
    vkb::SwapchainBuilder builder{context_.vkbDevice()};
    auto result = builder.set_old_swapchain(swapchain_.swapchain)
                      .set_desired_format({VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                      .add_fallback_format({VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
                      .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                      .set_desired_extent(width, height)
                      .build();
    if (!result) {
        throw std::runtime_error("Swapchain creation failed: " + result.error().message());
    }
    vkb::Swapchain old = swapchain_;
    swapchain_ = result.value();
    if (old.swapchain != VK_NULL_HANDLE) {
        vkb::destroy_swapchain(old);
    }
}

void Swapchain::createRenderPass()
{
    VkAttachmentDescription attachments[2]{};
    attachments[0].format = swapchain_.image_format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments[1].format = depthFormat_;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    const VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    // Orders this frame's color/depth writes after the previous frame's (the depth
    // image is shared by all frames in flight) and after image acquisition.
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    const VkPipelineStageFlags stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcStageMask = stages;
    dependency.srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = stages;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    info.attachmentCount = 2;
    info.pAttachments = attachments;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;
    VK_CHECK(vkCreateRenderPass(context_.device(), &info, nullptr, &renderPass_));
}

void Swapchain::createSizedResources()
{
    auto views = swapchain_.get_image_views();
    if (!views) {
        throw std::runtime_error("Swapchain image views failed: " + views.error().message());
    }
    imageViews_ = views.value();

    const VkExtent2D size = swapchain_.extent;

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = depthFormat_;
    imageInfo.extent = {size.width, size.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocationInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    VK_CHECK(vmaCreateImage(context_.allocator(), &imageInfo, &allocationInfo, &depthImage_, &depthAllocation_,
                            nullptr));

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = depthImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat_;
    viewInfo.subresourceRange = {depthAspect(depthFormat_), 0, 1, 0, 1};
    VK_CHECK(vkCreateImageView(context_.device(), &viewInfo, nullptr, &depthView_));

    framebuffers_.reserve(imageViews_.size());
    for (VkImageView colorView : imageViews_) {
        const VkImageView attachments[2] = {colorView, depthView_};
        VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        info.renderPass = renderPass_;
        info.attachmentCount = 2;
        info.pAttachments = attachments;
        info.width = size.width;
        info.height = size.height;
        info.layers = 1;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VK_CHECK(vkCreateFramebuffer(context_.device(), &info, nullptr, &framebuffer));
        framebuffers_.push_back(framebuffer);
    }
}

void Swapchain::destroySizedResources() noexcept
{
    const VkDevice device = context_.device();
    for (VkFramebuffer framebuffer : framebuffers_) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    framebuffers_.clear();
    if (depthView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, depthView_, nullptr);
        depthView_ = VK_NULL_HANDLE;
    }
    if (depthImage_ != VK_NULL_HANDLE) {
        vmaDestroyImage(context_.allocator(), depthImage_, depthAllocation_);
        depthImage_ = VK_NULL_HANDLE;
        depthAllocation_ = VK_NULL_HANDLE;
    }
    if (!imageViews_.empty()) {
        swapchain_.destroy_image_views(imageViews_);
        imageViews_.clear();
    }
}

void Swapchain::destroyAll() noexcept
{
    destroySizedResources();
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context_.device(), renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
    if (swapchain_.swapchain != VK_NULL_HANDLE) {
        vkb::destroy_swapchain(swapchain_);
        swapchain_ = vkb::Swapchain{};
    }
}

} // namespace render
```

- [ ] **Step 5: Write `CubeMesh`**

`src/render/CubeMesh.h`:
```cpp
#pragma once

#include <array>
#include <cstdint>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace render {

// Vertex and index buffers for scene::cubeVertices() / scene::cubeIndices().
class CubeMesh {
public:
    explicit CubeMesh(VmaAllocator allocator);
    ~CubeMesh();

    CubeMesh(const CubeMesh&) = delete;
    CubeMesh& operator=(const CubeMesh&) = delete;

    void bind(VkCommandBuffer commands) const;
    std::uint32_t indexCount() const;

    static VkVertexInputBindingDescription bindingDescription();
    static std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions();

private:
    struct Buffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
    };

    Buffer upload(const void* data, VkDeviceSize size, VkBufferUsageFlags usage);
    void destroy(Buffer& buffer) noexcept;

    VmaAllocator allocator_;
    Buffer vertices_;
    Buffer indices_;
};

} // namespace render
```

`src/render/CubeMesh.cpp`:
```cpp
#include "render/CubeMesh.h"

#include <cstddef>
#include <cstring>

#include "render/VkCheck.h"
#include "scene/CubeGeometry.h"

namespace render {

CubeMesh::CubeMesh(VmaAllocator allocator)
    : allocator_(allocator)
{
    const auto& vertices = scene::cubeVertices();
    const auto& indices = scene::cubeIndices();
    try {
        vertices_ = upload(vertices.data(), sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        indices_ = upload(indices.data(), sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    } catch (...) {
        destroy(vertices_);
        destroy(indices_);
        throw;
    }
}

CubeMesh::~CubeMesh()
{
    destroy(indices_);
    destroy(vertices_);
}

void CubeMesh::bind(VkCommandBuffer commands) const
{
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commands, 0, 1, &vertices_.buffer, &offset);
    vkCmdBindIndexBuffer(commands, indices_.buffer, 0, VK_INDEX_TYPE_UINT16);
}

std::uint32_t CubeMesh::indexCount() const
{
    return static_cast<std::uint32_t>(scene::cubeIndices().size());
}

VkVertexInputBindingDescription CubeMesh::bindingDescription()
{
    return {0, sizeof(scene::CubeVertex), VK_VERTEX_INPUT_RATE_VERTEX};
}

std::array<VkVertexInputAttributeDescription, 2> CubeMesh::attributeDescriptions()
{
    return {{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(scene::CubeVertex, position))},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<std::uint32_t>(offsetof(scene::CubeVertex, color))},
    }};
}

CubeMesh::Buffer CubeMesh::upload(const void* data, VkDeviceSize size, VkBufferUsageFlags usage)
{
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    Buffer result;
    VmaAllocationInfo info{};
    VK_CHECK(vmaCreateBuffer(allocator_, &bufferInfo, &allocationInfo, &result.buffer, &result.allocation, &info));
    std::memcpy(info.pMappedData, data, static_cast<std::size_t>(size));
    const VkResult flushed = vmaFlushAllocation(allocator_, result.allocation, 0, VK_WHOLE_SIZE);
    if (flushed != VK_SUCCESS) {
        destroy(result);
        throwVkError(flushed, "vmaFlushAllocation");
    }
    return result;
}

void CubeMesh::destroy(Buffer& buffer) noexcept
{
    if (buffer.buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
        buffer = Buffer{};
    }
}

} // namespace render
```

- [ ] **Step 6: Write `CubePipeline`**

`src/render/CubePipeline.h`:
```cpp
#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace render {

// Graphics pipeline for the cube: MVP push constant, depth test, back-face culling.
class CubePipeline {
public:
    CubePipeline(VkDevice device, VkRenderPass renderPass);
    ~CubePipeline();

    CubePipeline(const CubePipeline&) = delete;
    CubePipeline& operator=(const CubePipeline&) = delete;

    void bind(VkCommandBuffer commands) const;
    void pushMvp(VkCommandBuffer commands, const glm::mat4& mvp) const;

private:
    VkDevice device_;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};

} // namespace render
```

`src/render/CubePipeline.cpp`:
```cpp
#include "render/CubePipeline.h"

#include <cstddef>
#include <cstdint>

#include "render/CubeMesh.h"
#include "render/VkCheck.h"

namespace render {

namespace {

// SPIR-V generated at build time by glslc -mfmt=c.
const std::uint32_t kVertexSpirv[] =
#include "cube.vert.spv.inc"
    ;
const std::uint32_t kFragmentSpirv[] =
#include "cube.frag.spv.inc"
    ;

VkShaderModule createShaderModule(VkDevice device, const std::uint32_t* code, std::size_t bytes)
{
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = bytes;
    info.pCode = code;
    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &module));
    return module;
}

} // namespace

CubePipeline::CubePipeline(VkDevice device, VkRenderPass renderPass)
    : device_(device)
{
    const VkPushConstantRange pushRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    VK_CHECK(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &layout_));

    VkShaderModule vertexModule = VK_NULL_HANDLE;
    VkShaderModule fragmentModule = VK_NULL_HANDLE;
    VkResult result = VK_SUCCESS;
    try {
        vertexModule = createShaderModule(device_, kVertexSpirv, sizeof(kVertexSpirv));
        fragmentModule = createShaderModule(device_, kFragmentSpirv, sizeof(kFragmentSpirv));

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertexModule;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragmentModule;
        stages[1].pName = "main";

        const auto binding = CubeMesh::bindingDescription();
        const auto attributes = CubeMesh::attributeDescriptions();
        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterization{
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterization.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
        // Geometry is CCW from outside; the projection's Y flip keeps it CCW in framebuffer space.
        rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterization.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depth.depthTestEnable = VK_TRUE;
        depth.depthWriteEnable = VK_TRUE;
        depth.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1;
        blend.pAttachments = &blendAttachment;

        const VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamicStates;

        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = 2;
        info.pStages = stages;
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &inputAssembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &rasterization;
        info.pMultisampleState = &multisample;
        info.pDepthStencilState = &depth;
        info.pColorBlendState = &blend;
        info.pDynamicState = &dynamic;
        info.layout = layout_;
        info.renderPass = renderPass;
        info.subpass = 0;
        result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline_);
    } catch (...) {
        vkDestroyShaderModule(device_, vertexModule, nullptr);
        vkDestroyShaderModule(device_, fragmentModule, nullptr);
        vkDestroyPipelineLayout(device_, layout_, nullptr);
        throw;
    }
    vkDestroyShaderModule(device_, vertexModule, nullptr);
    vkDestroyShaderModule(device_, fragmentModule, nullptr);
    if (result != VK_SUCCESS) {
        vkDestroyPipelineLayout(device_, layout_, nullptr);
        throwVkError(result, "vkCreateGraphicsPipelines");
    }
}

CubePipeline::~CubePipeline()
{
    vkDestroyPipeline(device_, pipeline_, nullptr);
    vkDestroyPipelineLayout(device_, layout_, nullptr);
}

void CubePipeline::bind(VkCommandBuffer commands) const
{
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
}

void CubePipeline::pushMvp(VkCommandBuffer commands, const glm::mat4& mvp) const
{
    vkCmdPushConstants(commands, layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mvp);
}

} // namespace render
```

- [ ] **Step 7: Write `Renderer`**

`src/render/Renderer.h`:
```cpp
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "render/VulkanContext.h"

struct SDL_Window;

namespace render {

class CubeMesh;
class CubePipeline;
class Swapchain;

// Draws the cube into an SDL window with Vulkan. Throws std::runtime_error on failure.
class Renderer {
public:
    Renderer(SDL_Window* window, std::uint32_t width, std::uint32_t height, bool enableValidation);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Drawable size in physical pixels; a zero dimension pauses drawing.
    void resize(std::uint32_t width, std::uint32_t height);
    void draw(const glm::mat4& mvp);
    void waitIdle();

private:
    static constexpr std::uint32_t kFramesInFlight = 2;

    struct Frame {
        VkCommandBuffer commands = VK_NULL_HANDLE;
        VkSemaphore imageAcquired = VK_NULL_HANDLE;
        VkFence inFlight = VK_NULL_HANDLE;
    };

    void createFrameResources();
    void destroyFrameResources() noexcept;
    void createRenderFinishedSemaphores();
    void destroyRenderFinishedSemaphores() noexcept;
    // Returns false while the surface has zero size (e.g. minimized).
    bool recreateSwapchain();
    bool surfaceHasZeroExtent() const;
    void record(VkCommandBuffer commands, std::uint32_t imageIndex, const glm::mat4& mvp);

    VulkanContext context_;
    std::unique_ptr<Swapchain> swapchain_;
    std::unique_ptr<CubePipeline> pipeline_;
    std::unique_ptr<CubeMesh> mesh_;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::array<Frame, kFramesInFlight> frames_{};
    std::vector<VkSemaphore> renderFinished_; // one per swapchain image
    std::uint32_t frameIndex_ = 0;
    std::uint32_t width_;
    std::uint32_t height_;
    bool swapchainDirty_ = false;
};

} // namespace render
```

`src/render/Renderer.cpp`:
```cpp
#include "render/Renderer.h"

#include <exception>

#include "render/CubeMesh.h"
#include "render/CubePipeline.h"
#include "render/Swapchain.h"
#include "render/VkCheck.h"

namespace render {

Renderer::Renderer(SDL_Window* window, std::uint32_t width, std::uint32_t height, bool enableValidation)
    : context_(window, enableValidation)
    , width_(width)
    , height_(height)
{
    try {
        swapchain_ = std::make_unique<Swapchain>(context_, width, height);
        pipeline_ = std::make_unique<CubePipeline>(context_.device(), swapchain_->renderPass());
        mesh_ = std::make_unique<CubeMesh>(context_.allocator());
        createFrameResources();
        createRenderFinishedSemaphores();
    } catch (...) {
        destroyRenderFinishedSemaphores();
        destroyFrameResources();
        throw;
    }
}

Renderer::~Renderer()
{
    vkDeviceWaitIdle(context_.device());
    destroyFrameResources();
    mesh_.reset();
    pipeline_.reset();
    swapchain_.reset();
    // Present waits on these; destroy them only after the swapchain is gone.
    destroyRenderFinishedSemaphores();
}

void Renderer::resize(std::uint32_t width, std::uint32_t height)
{
    if (width == width_ && height == height_) {
        return;
    }
    width_ = width;
    height_ = height;
    swapchainDirty_ = true;
}

void Renderer::waitIdle()
{
    VK_CHECK(vkDeviceWaitIdle(context_.device()));
}

void Renderer::draw(const glm::mat4& mvp)
{
    if (width_ == 0 || height_ == 0) {
        return;
    }
    if (swapchainDirty_ && !recreateSwapchain()) {
        return;
    }

    const VkDevice device = context_.device();
    Frame& frame = frames_[frameIndex_];
    VK_CHECK(vkWaitForFences(device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX));

    std::uint32_t imageIndex = 0;
    const VkResult acquired = vkAcquireNextImageKHR(device, swapchain_->handle(), UINT64_MAX, frame.imageAcquired,
                                                    VK_NULL_HANDLE, &imageIndex);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchainDirty_ = true;
        return;
    }
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
        throwVkError(acquired, "vkAcquireNextImageKHR");
    }
    if (acquired == VK_SUBOPTIMAL_KHR) {
        swapchainDirty_ = true;
    }

    VK_CHECK(vkResetFences(device, 1, &frame.inFlight));
    VK_CHECK(vkResetCommandBuffer(frame.commands, 0));
    record(frame.commands, imageIndex, mvp);

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &frame.imageAcquired;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &frame.commands;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderFinished_[imageIndex];
    VK_CHECK(vkQueueSubmit(context_.graphicsQueue(), 1, &submit, frame.inFlight));

    const VkSwapchainKHR swapchain = swapchain_->handle();
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinished_[imageIndex];
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain;
    present.pImageIndices = &imageIndex;
    const VkResult presented = vkQueuePresentKHR(context_.presentQueue(), &present);
    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) {
        swapchainDirty_ = true;
    } else if (presented != VK_SUCCESS) {
        throwVkError(presented, "vkQueuePresentKHR");
    }

    frameIndex_ = (frameIndex_ + 1) % kFramesInFlight;
}

void Renderer::record(VkCommandBuffer commands, std::uint32_t imageIndex, const glm::mat4& mvp)
{
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(commands, &begin));

    const VkExtent2D extent = swapchain_->extent();
    VkClearValue clears[2]{};
    clears[0].color = {{0.02f, 0.02f, 0.025f, 1.0f}};
    clears[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = swapchain_->renderPass();
    pass.framebuffer = swapchain_->framebuffer(imageIndex);
    pass.renderArea = {{0, 0}, extent};
    pass.clearValueCount = 2;
    pass.pClearValues = clears;
    vkCmdBeginRenderPass(commands, &pass, VK_SUBPASS_CONTENTS_INLINE);

    pipeline_->bind(commands);
    const VkViewport viewport{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height),
                              0.0f, 1.0f};
    vkCmdSetViewport(commands, 0, 1, &viewport);
    const VkRect2D scissor{{0, 0}, extent};
    vkCmdSetScissor(commands, 0, 1, &scissor);
    mesh_->bind(commands);
    pipeline_->pushMvp(commands, mvp);
    vkCmdDrawIndexed(commands, mesh_->indexCount(), 1, 0, 0, 0);

    vkCmdEndRenderPass(commands);
    VK_CHECK(vkEndCommandBuffer(commands));
}

bool Renderer::surfaceHasZeroExtent() const
{
    VkSurfaceCapabilitiesKHR capabilities{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context_.physicalDevice(), context_.surface(), &capabilities));
    return capabilities.currentExtent.width == 0 || capabilities.currentExtent.height == 0;
}

// width_/height_ only gate the zero-size case: vk-bootstrap takes the extent from
// the surface whenever the platform reports one.
bool Renderer::recreateSwapchain()
{
    if (surfaceHasZeroExtent()) {
        return false;
    }
    VK_CHECK(vkDeviceWaitIdle(context_.device()));
    try {
        swapchain_->recreate(width_, height_);
    } catch (const std::exception&) {
        // The window may have been minimized after the check above; retry later.
        if (surfaceHasZeroExtent()) {
            return false;
        }
        throw;
    }
    destroyRenderFinishedSemaphores();
    createRenderFinishedSemaphores();
    swapchainDirty_ = false;
    return true;
}

void Renderer::createFrameResources()
{
    const VkDevice device = context_.device();

    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = context_.graphicsQueueFamily();
    VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool_));

    std::array<VkCommandBuffer, kFramesInFlight> buffers{};
    VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = kFramesInFlight;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocateInfo, buffers.data()));

    const VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (std::uint32_t i = 0; i < kFramesInFlight; ++i) {
        frames_[i].commands = buffers[i];
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frames_[i].imageAcquired));
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frames_[i].inFlight));
    }
}

void Renderer::destroyFrameResources() noexcept
{
    const VkDevice device = context_.device();
    for (Frame& frame : frames_) {
        if (frame.imageAcquired != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, frame.imageAcquired, nullptr);
        }
        if (frame.inFlight != VK_NULL_HANDLE) {
            vkDestroyFence(device, frame.inFlight, nullptr);
        }
        frame = Frame{};
    }
    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }
}

void Renderer::createRenderFinishedSemaphores()
{
    const VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    renderFinished_.assign(swapchain_->imageCount(), VK_NULL_HANDLE);
    for (VkSemaphore& semaphore : renderFinished_) {
        VK_CHECK(vkCreateSemaphore(context_.device(), &info, nullptr, &semaphore));
    }
}

void Renderer::destroyRenderFinishedSemaphores() noexcept
{
    for (VkSemaphore semaphore : renderFinished_) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(context_.device(), semaphore, nullptr);
        }
    }
    renderFinished_.clear();
}

} // namespace render
```

- [ ] **Step 8: Add the `render` part to `CMakeLists.txt`**

Inside the `if(CUBEAPP_BUILD_APP)` block written by Task 2, add exactly these lines (they match the
final file in Task 2 Step 1):

```cmake
    find_package(Vulkan REQUIRED COMPONENTS glslc)
    find_package(SDL3 CONFIG REQUIRED)
    find_package(vk-bootstrap CONFIG REQUIRED)
    find_package(VulkanMemoryAllocator CONFIG REQUIRED)

    # GLSL -> SPIR-V emitted as C initializer lists, #included by CubePipeline.cpp.
    set(CUBEAPP_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
    set(CUBEAPP_SHADER_OUTPUTS)
    foreach(shader cube.vert cube.frag)
        set(output ${CUBEAPP_SHADER_DIR}/${shader}.spv.inc)
        add_custom_command(
            OUTPUT ${output}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CUBEAPP_SHADER_DIR}
            COMMAND Vulkan::glslc --target-env=vulkan1.1 -mfmt=c -o ${output}
                    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}
            COMMENT "Compiling shader ${shader}"
            VERBATIM)
        list(APPEND CUBEAPP_SHADER_OUTPUTS ${output})
    endforeach()

    add_library(render STATIC
        src/render/CubeMesh.cpp
        src/render/CubePipeline.cpp
        src/render/Log.cpp
        src/render/Renderer.cpp
        src/render/Swapchain.cpp
        src/render/VkCheck.cpp
        src/render/VulkanContext.cpp
        src/render/vma_impl.cpp
        ${CUBEAPP_SHADER_OUTPUTS}
    )
    target_include_directories(render PRIVATE ${CUBEAPP_SHADER_DIR})
    target_link_libraries(render PUBLIC
        scene
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
        vk-bootstrap::vk-bootstrap
        SDL3::SDL3
    )
    cubeapp_set_warnings(render)
```

- [ ] **Step 9: Build the `render` target**

Requires Task 1's SDL3, vk-bootstrap and VMA installs (wxWidgets is not needed yet).
```powershell
cmake --preset vs2022 -DCUBEAPP_BUILD_APP=ON
cmake --build --preset vs2022-debug --target render
cmake --build --preset vs2022-release --target render
```
Expected: shaders compile (`Compiling shader cube.vert/frag`), `render.lib` builds in both configs with zero warnings.

- [ ] **Step 10: Commit**

```powershell
git add CMakeLists.txt shaders src/render
git commit -m "feat: add Vulkan renderer (vk-bootstrap, VMA, swapchain, cube pipeline)"
```

---

### Task 4: Platform embedding and wxWidgets application

**Files:**
- Create: `src/platform/embed.h`, `src/platform/embed_detail.h`, `src/platform/embed_common.cpp`, `src/platform/embed_win32.cpp`, `src/platform/embed_x11.cpp`, `src/platform/embed_cocoa.mm`
- Create: `src/app/App.h|.cpp`, `src/app/MainFrame.h|.cpp`, `src/app/RenderPanel.h|.cpp`, `src/app/cubeapp.rc`

**Interfaces:**
- Consumes: `render::Renderer` (Task 3); `scene::SpinController`, `scene::Camera`, `scene::FpsCounter` (Task 2).
- Produces: `platform::prepareEnvironment()`, `platform::embedNativeWindow(wxWindow&) -> SDL_Window*`; executable `cubeapp`.

- [ ] **Step 1: Write platform embedding**

`src/platform/embed.h`:
```cpp
#pragma once

struct SDL_Window;
class wxWindow;

namespace platform {

// Adjusts the process environment before any toolkit initializes.
// Linux forces the X11 backends (unless the user set them); elsewhere a no-op.
void prepareEnvironment();

// Wraps the native window of `window` in an SDL window usable for Vulkan.
// SDL video must be initialized. Throws std::runtime_error on failure.
SDL_Window* embedNativeWindow(wxWindow& window);

} // namespace platform
```

`src/platform/embed_detail.h`:
```cpp
#pragma once

#include <SDL3/SDL_properties.h>

struct SDL_Window;

namespace platform::detail {

// Takes ownership of `props`: adds the Vulkan flag, creates the window,
// destroys the properties. Throws std::runtime_error on failure.
SDL_Window* createEmbeddedWindow(SDL_PropertiesID props);

// Creates an empty property set or throws.
SDL_PropertiesID createProperties();

} // namespace platform::detail
```

`src/platform/embed_common.cpp`:
```cpp
#include "platform/embed_detail.h"

#include <stdexcept>
#include <string>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_video.h>

namespace platform::detail {

SDL_PropertiesID createProperties()
{
    const SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0) {
        throw std::runtime_error(std::string("SDL_CreateProperties failed: ") + SDL_GetError());
    }
    return props;
}

SDL_Window* createEmbeddedWindow(SDL_PropertiesID props)
{
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true);
    SDL_Window* window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (!window) {
        throw std::runtime_error(std::string("Embedding the render window in SDL failed: ") + SDL_GetError());
    }
    return window;
}

} // namespace platform::detail
```

`src/platform/embed_win32.cpp`:
```cpp
#include "platform/embed.h"

#include <wx/window.h>

#include <SDL3/SDL_video.h>

#include "platform/embed_detail.h"

namespace platform {

void prepareEnvironment()
{
}

SDL_Window* embedNativeWindow(wxWindow& window)
{
    const SDL_PropertiesID props = detail::createProperties();
    SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, window.GetHandle());
    return detail::createEmbeddedWindow(props);
}

} // namespace platform
```

`src/platform/embed_x11.cpp`:
```cpp
#include "platform/embed.h"

#include <cstdlib>
#include <stdexcept>

#include <wx/window.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_video.h>

#include "platform/embed_detail.h"

namespace platform {

void prepareEnvironment()
{
    // Respect values the user already set.
    setenv("GDK_BACKEND", "x11", 0);
    setenv("SDL_VIDEO_DRIVER", "x11", 0);
}

SDL_Window* embedNativeWindow(wxWindow& window)
{
    GtkWidget* widget = static_cast<GtkWidget*>(window.GetHandle());
    gtk_widget_realize(widget);
    GdkWindow* gdkWindow = window.GTKGetDrawingWindow();
    if (!gdkWindow) {
        gdkWindow = gtk_widget_get_window(widget);
    }
    if (!gdkWindow || !GDK_IS_X11_WINDOW(gdkWindow)) {
        throw std::runtime_error("The render panel has no X11 window; run under X11 or XWayland");
    }
    gdk_window_ensure_native(gdkWindow);
    // SDL uses its own X connection; make sure the server has created the window first.
    gdk_display_sync(gdk_window_get_display(gdkWindow));
    const auto xid = gdk_x11_window_get_xid(gdkWindow);

    // wxWidgets keeps handling mouse input; SDL must not select input on this window.
    SDL_SetHint(SDL_HINT_VIDEO_X11_EXTERNAL_WINDOW_INPUT, "0");

    const SDL_PropertiesID props = detail::createProperties();
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER, static_cast<Sint64>(xid));
    return detail::createEmbeddedWindow(props);
}

} // namespace platform
```

`src/platform/embed_cocoa.mm`:
```objc
#include "platform/embed.h"

#include <wx/window.h>

#import <Cocoa/Cocoa.h>

#include <SDL3/SDL_video.h>

#include "platform/embed_detail.h"

namespace platform {

void prepareEnvironment()
{
}

SDL_Window* embedNativeWindow(wxWindow& window)
{
    NSView* view = window.GetHandle();
    const SDL_PropertiesID props = detail::createProperties();
    SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_WINDOW_POINTER, (__bridge void*)[view window]);
    SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_COCOA_VIEW_POINTER, (__bridge void*)view);
    return detail::createEmbeddedWindow(props);
}

} // namespace platform
```

- [ ] **Step 2: Write `App`**

`src/app/App.h`:
```cpp
#pragma once

#include <wx/app.h>

class App : public wxApp {
public:
    App();

    bool OnInit() override;
    int OnRun() override;

    // Makes the process exit with a non-zero code once the main loop ends.
    void markFailed() { failed_ = true; }

private:
    bool failed_ = false;
};

wxDECLARE_APP(App);
```

`src/app/App.cpp`:
```cpp
#include "app/App.h"

#include "app/MainFrame.h"
#include "platform/embed.h"

wxIMPLEMENT_APP(App);

// The application object is constructed before the toolkit initializes, so
// environment overrides (Linux X11 backend) still take effect here.
App::App()
{
    platform::prepareEnvironment();
}

bool App::OnInit()
{
    if (!wxApp::OnInit()) {
        return false;
    }
    auto* frame = new MainFrame();
    frame->Show();
    return true;
}

int App::OnRun()
{
    const int code = wxApp::OnRun();
    return failed_ ? 1 : code;
}
```

- [ ] **Step 3: Write `RenderPanel`**

`src/app/RenderPanel.h`:
```cpp
#pragma once

#include <chrono>
#include <exception>
#include <functional>
#include <memory>

#include <wx/window.h>

#include "scene/Camera.h"
#include "scene/FpsCounter.h"
#include "scene/SpinController.h"

struct SDL_Window;

namespace render {
class Renderer;
}

// Native child window hosting the SDL3/Vulkan view. Translates wx input into
// scene updates and drives one frame per idle event.
class RenderPanel : public wxWindow {
public:
    explicit RenderPanel(wxWindow* parent);
    ~RenderPanel() override;

    void setSpinning(bool spinning) { spin_.setSpinning(spinning); }
    bool isSpinning() const { return spin_.isSpinning(); }

    void setFpsCallback(std::function<void(double)> callback) { onFps_ = std::move(callback); }
    void setErrorCallback(std::function<void(const wxString&)> callback) { onError_ = std::move(callback); }

private:
    void onIdle(wxIdleEvent& event);
    void onPaint(wxPaintEvent& event);
    void onSize(wxSizeEvent& event);
    void onLeftDown(wxMouseEvent& event);
    void onLeftUp(wxMouseEvent& event);
    void onMotion(wxMouseEvent& event);
    void onWheel(wxMouseEvent& event);
    void onCaptureLost(wxMouseCaptureLostEvent& event);

    bool canRender() const;
    bool ensureRenderer();
    void renderFrame();
    void finishDrag();
    void shutdownRenderer() noexcept;
    void fail(const char* what, const std::exception& error);
    wxSize pixelSize() const;
    double now() const;

    std::function<void(double)> onFps_;
    std::function<void(const wxString&)> onError_;

    scene::SpinController spin_;
    scene::Camera camera_;
    scene::FpsCounter fps_;
    const std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now();
    double lastFrameTime_ = -1.0;

    bool sdlStarted_ = false;
    SDL_Window* sdlWindow_ = nullptr;
    std::unique_ptr<render::Renderer> renderer_;
    bool failed_ = false;
};
```

`src/app/RenderPanel.cpp`:
```cpp
#include "app/RenderPanel.h"

#include <algorithm>
#include <stdexcept>
#include <string>

#include <wx/dcclient.h>
#include <wx/toplevel.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>

#include <glm/gtc/quaternion.hpp>

#include "platform/embed.h"
#include "render/Renderer.h"

namespace {

#ifdef NDEBUG
constexpr bool kEnableValidation = false;
#else
constexpr bool kEnableValidation = true;
#endif

constexpr float kMaxFrameTime = 0.1f; // seconds; avoids a jump after stalls

glm::vec2 toVec(const wxPoint& point)
{
    return {static_cast<float>(point.x), static_cast<float>(point.y)};
}

} // namespace

RenderPanel::RenderPanel(wxWindow* parent)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
#ifdef __WXGTK__
    // GTK's double buffering would paint over the Vulkan output (as for wxGLCanvas).
    SetDoubleBuffered(false);
#endif

    Bind(wxEVT_IDLE, &RenderPanel::onIdle, this);
    Bind(wxEVT_PAINT, &RenderPanel::onPaint, this);
    Bind(wxEVT_SIZE, &RenderPanel::onSize, this);
    Bind(wxEVT_LEFT_DOWN, &RenderPanel::onLeftDown, this);
    Bind(wxEVT_LEFT_DCLICK, &RenderPanel::onLeftDown, this); // fast re-grab arrives as a double click
    Bind(wxEVT_LEFT_UP, &RenderPanel::onLeftUp, this);
    Bind(wxEVT_MOTION, &RenderPanel::onMotion, this);
    Bind(wxEVT_MOUSEWHEEL, &RenderPanel::onWheel, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &RenderPanel::onCaptureLost, this);
}

RenderPanel::~RenderPanel()
{
    shutdownRenderer();
}

void RenderPanel::onIdle(wxIdleEvent& event)
{
    renderFrame();
    if (canRender()) {
        event.RequestMore();
    }
}

void RenderPanel::onPaint(wxPaintEvent&)
{
    wxPaintDC dc(this); // validates the region; all drawing is done by Vulkan
    renderFrame();
}

void RenderPanel::onSize(wxSizeEvent& event)
{
    event.Skip();
    const wxSize size = pixelSize();
    camera_.setViewport(size.x, size.y);
    if (renderer_) {
        renderer_->resize(static_cast<std::uint32_t>(std::max(size.x, 0)),
                          static_cast<std::uint32_t>(std::max(size.y, 0)));
        renderFrame(); // keeps the view live during modal resize loops
    }
}

void RenderPanel::onLeftDown(wxMouseEvent& event)
{
    SetFocus();
    if (!HasCapture()) {
        CaptureMouse();
    }
    spin_.beginDrag(toVec(event.GetPosition()), now());
}

void RenderPanel::onLeftUp(wxMouseEvent&)
{
    finishDrag();
}

void RenderPanel::onMotion(wxMouseEvent& event)
{
    if (!spin_.isDragging()) {
        return;
    }
    if (!event.LeftIsDown()) {
        finishDrag();
        return;
    }
    spin_.dragTo(toVec(event.GetPosition()), now());
}

void RenderPanel::onWheel(wxMouseEvent& event)
{
    const int delta = event.GetWheelDelta();
    if (delta == 0 || event.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL) {
        return;
    }
    camera_.zoom(static_cast<float>(event.GetWheelRotation()) / static_cast<float>(delta));
}

void RenderPanel::onCaptureLost(wxMouseCaptureLostEvent&)
{
    if (spin_.isDragging()) {
        spin_.endDrag(now());
    }
}

void RenderPanel::finishDrag()
{
    if (spin_.isDragging()) {
        spin_.endDrag(now());
    }
    if (HasCapture()) {
        ReleaseMouse();
    }
}

bool RenderPanel::canRender() const
{
    if (failed_ || !IsShownOnScreen()) {
        return false;
    }
    const auto* topLevel = wxDynamicCast(wxGetTopLevelParent(const_cast<RenderPanel*>(this)), wxTopLevelWindow);
    if (topLevel && topLevel->IsIconized()) {
        return false;
    }
    const wxSize size = pixelSize();
    return size.x > 0 && size.y > 0;
}

bool RenderPanel::ensureRenderer()
{
    if (renderer_) {
        return true;
    }
    if (!canRender()) {
        return false;
    }
    const wxSize size = pixelSize();
    try {
        if (!sdlStarted_) {
            // wxWidgets owns mouse capture, and SDL must not block the screensaver.
            SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
            SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
            if (!SDL_Init(SDL_INIT_VIDEO)) {
                throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
            }
            sdlStarted_ = true;
        }
        sdlWindow_ = platform::embedNativeWindow(*this);
        renderer_ = std::make_unique<render::Renderer>(sdlWindow_, static_cast<std::uint32_t>(size.x),
                                                       static_cast<std::uint32_t>(size.y), kEnableValidation);
        camera_.setViewport(size.x, size.y);
        return true;
    } catch (const std::exception& error) {
        fail("Vulkan initialisation failed", error);
        return false;
    }
}

void RenderPanel::renderFrame()
{
    if (!canRender() || !ensureRenderer()) {
        return;
    }
    const double time = now();
    const float dt = lastFrameTime_ < 0.0 ? 0.0f : static_cast<float>(time - lastFrameTime_);
    lastFrameTime_ = time;
    spin_.update(std::min(dt, kMaxFrameTime));

    try {
        renderer_->draw(camera_.viewProjection() * glm::mat4_cast(spin_.orientation()));
    } catch (const std::exception& error) {
        fail("Rendering failed", error);
        return;
    }

    // SDL queues events for the wrapped window that this app never reads.
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);

    if (fps_.frame(time) && onFps_) {
        onFps_(fps_.fps());
    }
}

void RenderPanel::shutdownRenderer() noexcept
{
    renderer_.reset(); // waits for the GPU, then destroys Vulkan objects
    if (sdlWindow_) {
        SDL_DestroyWindow(sdlWindow_);
        sdlWindow_ = nullptr;
    }
    if (sdlStarted_) {
        SDL_Quit();
        sdlStarted_ = false;
    }
}

void RenderPanel::fail(const char* what, const std::exception& error)
{
    failed_ = true;
    shutdownRenderer();
    if (onError_) {
        onError_(wxString::Format("%s:\n%s", what, error.what()));
    }
}

wxSize RenderPanel::pixelSize() const
{
    return ToPhys(GetClientSize());
}

double RenderPanel::now() const
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime_).count();
}
```

- [ ] **Step 4: Write `MainFrame` and Windows resources**

`src/app/MainFrame.h`:
```cpp
#pragma once

#include <wx/frame.h>

class wxButton;
class RenderPanel;

// Main window: render panel, bottom bar with the Start/Stop button, status bar.
class MainFrame : public wxFrame {
public:
    MainFrame();

private:
    void onToggleSpin(wxCommandEvent& event);
    void showFps(double fps);
    void showSpinState();
    void reportFatalError(const wxString& message);

    RenderPanel* renderPanel_ = nullptr;
    wxButton* toggleButton_ = nullptr;
};
```

`src/app/MainFrame.cpp`:
```cpp
#include "app/MainFrame.h"

#include <wx/button.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>

#include "app/App.h"
#include "app/RenderPanel.h"

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "wxWidgets + SDL3 + Vulkan Cube")
{
    renderPanel_ = new RenderPanel(this);

    auto* bar = new wxPanel(this);
    // Created with the longer label so the button is wide enough for both.
    toggleButton_ = new wxButton(bar, wxID_ANY, "Start");
    auto* barSizer = new wxBoxSizer(wxHORIZONTAL);
    barSizer->AddStretchSpacer();
    barSizer->Add(toggleButton_, wxSizerFlags().Border(wxALL, FromDIP(8)));
    barSizer->AddStretchSpacer();
    bar->SetSizer(barSizer);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(renderPanel_, wxSizerFlags(1).Expand());
    sizer->Add(bar, wxSizerFlags().Expand());
    SetSizer(sizer);

    CreateStatusBar(2);
    SetStatusText("FPS: --", 0);
    showSpinState();

    toggleButton_->Bind(wxEVT_BUTTON, &MainFrame::onToggleSpin, this);
    renderPanel_->setFpsCallback([this](double fps) { showFps(fps); });
    renderPanel_->setErrorCallback([this](const wxString& message) { reportFatalError(message); });

    SetMinSize(FromDIP(wxSize(320, 240)));
    SetClientSize(FromDIP(wxSize(960, 640)));
    Centre();
}

void MainFrame::onToggleSpin(wxCommandEvent&)
{
    renderPanel_->setSpinning(!renderPanel_->isSpinning());
    showSpinState();
}

void MainFrame::showSpinState()
{
    const bool spinning = renderPanel_->isSpinning();
    toggleButton_->SetLabel(spinning ? "Stop" : "Start");
    SetStatusText(spinning ? "Spinning" : "Stopped", 1);
}

void MainFrame::showFps(double fps)
{
    SetStatusText(wxString::Format("FPS: %.1f", fps), 0);
}

void MainFrame::reportFatalError(const wxString& message)
{
    CallAfter([this, message] {
        wxMessageBox(message, "Rendering failed", wxOK | wxICON_ERROR, this);
        wxGetApp().markFailed();
        Close(true);
    });
}
```

`src/app/cubeapp.rc`:
```rc
// Common-controls v6 and per-monitor-v2 DPI awareness manifest from wxWidgets.
// wx.rc only emits the manifest when wxUSE_RC_MANIFEST is 1 (not set by default for MSVC).
#define wxUSE_RC_MANIFEST 1
#define wxUSE_DPI_AWARE_MANIFEST 2
#include "wx/msw/wx.rc"
```

- [ ] **Step 5a: Add the `cubeapp` part to `CMakeLists.txt`**

Inside `if(CUBEAPP_BUILD_APP)`: add `find_package(wxWidgets 3.2 CONFIG REQUIRED COMPONENTS core base)`
after the `VulkanMemoryAllocator` `find_package`, and append everything from `if(WIN32)` through
`set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT cubeapp)` after `cubeapp_set_warnings(render)`,
exactly as in the final file shown in Task 2 Step 1 (read the Task 2 brief, Step 1).

Also add, right after `include(CTest)` (before any `find_package`), so RelWithDebInfo/MinSizeRel
builds use the Release dependencies instead of Debug ones (MSVC runtime mismatch):

```cmake
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Release)
```

And in `CMakePresets.json`, give the hidden `base` configure preset
`"cacheVariables": { "CMAKE_DISABLE_FIND_PACKAGE_VulkanHeaders": "ON" }` (silences the
`VulkanHeaders` warning from vk-bootstrap's package config; it falls back to FindVulkan).

- [ ] **Step 5: Configure and build everything (Debug and Release)**

```powershell
cmake --preset vs2022
cmake --build --preset vs2022-debug
cmake --build --preset vs2022-release
ctest --preset vs2022-debug
ctest --preset vs2022-release
```
Expected: zero warnings, zero errors; `build\vs2022\Debug\cubeapp.exe` and `build\vs2022\Release\cubeapp.exe` exist; tests 100% pass.

- [ ] **Step 6: Smoke run**

```powershell
$p = Start-Process -FilePath build\vs2022\Debug\cubeapp.exe -PassThru
Start-Sleep -Seconds 5
$p.HasExited   # Expected: False (app still running, no fatal error dialog closed it)
Stop-Process -Id $p.Id
```

- [ ] **Step 7: Commit**

```powershell
git add CMakeLists.txt src/platform src/app
git commit -m "feat: add wxWidgets main window with SDL3-embedded Vulkan render panel"
```

---

### Task 5: Runtime verification on Windows, README

**Files:**
- Create: `README.md`
- Create (scratch, not committed): verification scripts in the agent's scratchpad directory.

- [ ] **Step 1: Automated visual check**

Write a PowerShell script in the scratchpad that: starts `build\vs2022\Debug\cubeapp.exe`; waits 3 s; finds the main window (`Get-Process cubeapp | % MainWindowHandle`); captures a screenshot of the window rectangle with `System.Drawing` (`Graphics.CopyFromScreen`) to a PNG in the scratchpad. Inspect the PNG (Read tool): a coloured cube on a dark background, a `Stop` button at the bottom centre, status bar `FPS: <number>` and `Spinning`.

- [ ] **Step 2: Automated interaction check**

Extend the script using `user32.dll` P/Invoke (`SetCursorPos`, `mouse_event`) to:
1. Click the button → screenshot → status shows `Stopped`, button `Start`; two screenshots 1 s apart are identical in the render area (cube not moving).
2. Click again → `Spinning`.
3. Left-drag fast horizontally across the render area (≥ 200 px in ≤ 100 ms) and release → the cube keeps spinning about a vertical axis (successive screenshots show horizontal rotation).
4. Wheel up 10 notches (`MOUSEEVENTF_WHEEL`, +120 each) → cube appears larger; wheel down 30 notches → smaller (clamped, no disappearance).
5. Resize the window (`MoveWindow`) to 500×400 and 1400×900, maximize (`ShowWindow` SW_MAXIMIZE), minimize (SW_MINIMIZE) for 2 s, restore (SW_RESTORE) → after each, screenshot shows a correctly proportioned cube, process alive.
6. Close via `CloseMainWindow()` → process exits with code 0 within 5 s.

- [ ] **Step 3: Validation layer check**

Run Debug under the validation layer capturing stderr: `Start-Process build\vs2022\Debug\cubeapp.exe -RedirectStandardError <scratch>\stderr.txt`, exercise as in Step 2 (resize + minimize), close. Expected: no lines starting with `[vulkan]` of error severity in `stderr.txt`. (If GUI-subsystem stderr redirection yields nothing, set `VK_LAYER_SETTINGS_PATH`-free alternative: run with environment `VK_LOADER_DEBUG=error` and inspect; or temporarily attach via `DebugView`. Report what was observed.)

- [ ] **Step 4: Write `README.md`**

Contents: one-paragraph description; features list; prerequisites per OS (CMake ≥ 3.24, Vulkan SDK ≥ 1.3.275, VS 2022/2026 with C++ desktop workload; Linux: GCC/Clang C++20, Ninja, `libgtk-3-dev`, `libx11-dev`, `libvulkan-dev`, `glslc`, plus SDL3 X11 build deps; macOS: Xcode CLT, Ninja, LunarG Vulkan SDK with MoltenVK); build steps per OS:

Windows:
```powershell
cmake -S deps --preset vs2022
cmake --build deps/build/vs2022 --config Debug
cmake --build deps/build/vs2022 --config Release
cmake --preset vs2022
cmake --build --preset vs2022-debug
ctest --preset vs2022-debug
.\build\vs2022\Debug\cubeapp.exe
```
Linux/macOS:
```sh
cmake -S deps --preset ninja
cmake --build deps/build/ninja
cmake --preset ninja-release
cmake --build --preset ninja-release
ctest --preset ninja-release
./build/ninja-release/cubeapp            # macOS: open build/ninja-release/cubeapp.app (source the Vulkan SDK setup-env.sh first)
```
Controls (button, drag/flick, wheel, resize); using system packages instead of `deps/` (`-DCMAKE_PREFIX_PATH=...`); platform status table: Windows verified; Linux (X11/XWayland) per WSL2 result; macOS not yet verified.

- [ ] **Step 5: Commit**

```powershell
git add README.md
git commit -m "docs: add README with build instructions and platform status"
```

---

### Task 6: Linux build and run under WSL2 (after Windows is verified)

- [ ] **Step 1:** Check `wsl -l -v`. If no distro is installed, report to the user instead of installing one (installing a distro is an outward-facing system change).
- [ ] **Step 2:** Inside WSL (clone of the repo via `git clone` from the Windows path into the Linux home, so builds run on ext4), install build deps with `sudo apt-get install` (requires the user's sudo password — if not available non-interactively, stop and report the exact command list).
- [ ] **Step 3:** `cmake -S deps --preset ninja && cmake --build deps/build/ninja`, then `cmake --preset ninja-release && cmake --build --preset ninja-release && ctest --preset ninja-release`.
- [ ] **Step 4:** Run `./build/ninja-release/cubeapp` under WSLg (XWayland). Vulkan in WSL2 uses Mesa `dzn` (D3D12) or `lavapipe` (CPU); either is acceptable. Screenshot via the Windows-side capture script. Report results and update the README platform table.
- [ ] **Step 5:** Commit any portability fixes (`fix(linux): ...`).
```

---

### Task 7: macOS (only after the user approves the Windows build)

Known issues from the pre-execution reviews; the approach must be agreed with the user first:

- **Critical:** SDL 3.2.30's Cocoa backend, when wrapping an existing `NSWindow`, calls
  `[nswindow setContentView:nsview]` (`src/video/cocoa/SDL_cocoawindow.m`), so wrapping the wx
  panel's view would replace the whole frame content and hook the window's event chain. Likely fix:
  on macOS skip SDL window wrapping and create the surface directly from a `CAMetalLayer` on the
  panel's `NSView` via `vkCreateMetalSurfaceEXT` (MoltenVK), which means `Renderer`/`VulkanContext`
  take a surface-creation callback (plus the instance extension list) instead of an `SDL_Window*`.
- **Minor:** set `wantsLayer = YES` on the view and `layer.contentsScale = window.backingScaleFactor`
  (update on backing-scale changes) so Retina renders at full resolution.
