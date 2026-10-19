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

# Build RelWithDebInfo/MinSizeRel against the Release dependencies (matching MSVC runtime).
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Release)

function(cubeapp_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive->")
    else()
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wall;-Wextra;-Wpedantic;-Werror;-Wno-missing-field-initializers>")
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
    # Find modules (FindVulkan, FindX11) create imported targets with only a configuration-less IMPORTED_LOCATION;
    # mapping RelWithDebInfo/MinSizeRel to Release would make them unresolvable, so the mapping applies only to the
    # config packages and is unset around the find modules.
    set(_cubeapp_map_relwithdebinfo ${CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO})
    set(_cubeapp_map_minsizerel ${CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL})
    unset(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO)
    unset(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL)
    find_package(Vulkan REQUIRED COMPONENTS glslc)
    if(UNIX AND NOT APPLE)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
        find_package(X11 REQUIRED)
    endif()
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO ${_cubeapp_map_relwithdebinfo})
    set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL ${_cubeapp_map_minsizerel})
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
        target_link_libraries(cubeapp PRIVATE PkgConfig::GTK3 X11::X11)
    endif()

    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT cubeapp)
endif()
