# raymotion - Remotion-style motion graphics engine on raylib + ffmpeg
cmake_minimum_required(VERSION 3.21)
project(raymotion LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(RAYMOTION_BUILD_EXAMPLES "Build the showcase and starter projects" ${PROJECT_IS_TOP_LEVEL})
set(RAYMOTION_RAYLIB "auto" CACHE STRING "raylib source: auto, vendored (prebuilt MSVC lib), fetch (build 6.2 from source) or system (find_package)")
set_property(CACHE RAYMOTION_RAYLIB PROPERTY STRINGS auto vendored fetch system)

set(RAYMOTION_VENDORED_RAYLIB "${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib")
set(_raylib_mode "${RAYMOTION_RAYLIB}")
if(_raylib_mode STREQUAL "auto")
  if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8 AND EXISTS "${RAYMOTION_VENDORED_RAYLIB}/lib/raylib.lib")
    set(_raylib_mode vendored)
  else()
    set(_raylib_mode fetch)
  endif()
endif()

if(MSVC)
  # The prebuilt raylib.lib is the release DLL runtime (/MD); a raylib built from source
  # follows the configuration.
  if(_raylib_mode STREQUAL "vendored")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
  else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  endif()
  add_compile_options(/utf-8)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/msvc-sdk-libs/kernel32.lib")
    include(cmake/LocalMsvcToolchain.cmake)
  endif()
endif()

# ---------------------------------------------------------------- raylib
message(STATUS "raymotion: raylib from ${_raylib_mode}")
if(_raylib_mode STREQUAL "vendored")
  add_library(raylib STATIC IMPORTED GLOBAL)
  set_target_properties(raylib PROPERTIES
    IMPORTED_LOCATION "${RAYMOTION_VENDORED_RAYLIB}/lib/raylib.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${RAYMOTION_VENDORED_RAYLIB}/include"
    INTERFACE_LINK_LIBRARIES "winmm;gdi32;user32;shell32")
elseif(_raylib_mode STREQUAL "system")
  find_package(raylib 6.2 REQUIRED)
else()
  include(FetchContent)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/6.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(raylib)
endif()

# ---------------------------------------------------------------- engine library
add_library(rmtn_engine STATIC
  src/engine/cli.cpp
  src/engine/frame_stream.cpp
  src/engine/fonts.cpp
  src/engine/render.cpp
  src/engine/filters.cpp
  src/engine/anim.cpp
  src/engine/layout.cpp
  src/engine/json.cpp
  src/engine/scene3d.cpp
  src/engine/media.cpp
  src/engine/prefab.cpp
  src/engine/diagram_layout.cpp
  src/engine/diagram_parse.cpp
  src/engine/diagram_draw.cpp
  src/engine/bpmn.cpp
  src/engine/chem.cpp
  src/engine/chem3d.cpp
  src/engine/particles.cpp
  src/engine/paths.cpp)
add_library(raymotion::engine ALIAS rmtn_engine)
target_include_directories(rmtn_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
# Single-header decoders (stb_image, nanosvg), compiled privately into media.cpp.
target_include_directories(rmtn_engine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_link_libraries(rmtn_engine PUBLIC raylib)
# Development fallback for bundled fonts; installed apps ship assets/ beside the executable.
target_compile_definitions(rmtn_engine PRIVATE RMTN_BUNDLED_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/assets")

# ---------------------------------------------------------------- example projects
if(RAYMOTION_BUILD_EXAMPLES)
  add_executable(raymotion
    examples/showcase/main.cpp
    examples/showcase/scenes.cpp
    examples/showcase/fx_present.cpp
    examples/showcase/ht_explainer.cpp
    examples/showcase/ht_plain.cpp
    examples/showcase/ht_feed.cpp
    examples/showcase/gpu_scene.cpp
    examples/showcase/prefabs.cpp
    examples/showcase/asp.cpp
    examples/showcase/tokens.cpp
    examples/showcase/own_compute.cpp
    examples/showcase/promo.cpp
    examples/showcase/npr.cpp
    examples/showcase/diagrams.cpp
    examples/showcase/particles.cpp
    examples/showcase/paths.cpp)
  target_link_libraries(raymotion PRIVATE rmtn_engine)

  add_executable(raymotion_starter examples/starter/main.cpp)
  target_link_libraries(raymotion_starter PRIVATE rmtn_engine)
endif()

# ---------------------------------------------------------------- tests
if(PROJECT_IS_TOP_LEVEL)
  include(CTest)
endif()
if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING AND RAYMOTION_BUILD_EXAMPLES)
  # Pure logic: runs anywhere.
  add_executable(raymotion_ht_tests tests/ht_logic_tests.cpp)
  add_test(NAME ht_semantics COMMAND raymotion_ht_tests)
  add_executable(raymotion_anim_tests tests/anim_tests.cpp)
  target_link_libraries(raymotion_anim_tests PRIVATE rmtn_engine)
  add_test(NAME anim_semantics COMMAND raymotion_anim_tests)
  add_executable(raymotion_diagram_tests tests/diagram_tests.cpp)
  target_link_libraries(raymotion_diagram_tests PRIVATE rmtn_engine)
  add_test(NAME diagram_parsing COMMAND raymotion_diagram_tests)
  add_executable(raymotion_particle_tests tests/particle_tests.cpp)
  target_link_libraries(raymotion_particle_tests PRIVATE rmtn_engine)
  add_test(NAME particles COMMAND raymotion_particle_tests)
  add_executable(raymotion_path_tests tests/path_tests.cpp)
  target_link_libraries(raymotion_path_tests PRIVATE rmtn_engine)
  add_test(NAME paths COMMAND raymotion_path_tests)

  # The rest need an OpenGL 3.3 context (label "gl"; on headless Linux run under xvfb).
  add_executable(raymotion_render_tests tests/render_tests.cpp)
  target_link_libraries(raymotion_render_tests PRIVATE rmtn_engine)
  add_test(NAME renderer_regressions COMMAND raymotion_render_tests)

  add_executable(raymotion_layer_tests tests/layer_tests.cpp)
  target_link_libraries(raymotion_layer_tests PRIVATE rmtn_engine)
  add_test(NAME layer_compositing COMMAND raymotion_layer_tests)

  add_executable(raymotion_scene3d_tests tests/scene3d_tests.cpp)
  target_link_libraries(raymotion_scene3d_tests PRIVATE rmtn_engine)
  add_test(NAME scene3d COMMAND raymotion_scene3d_tests)

  add_executable(raymotion_media_tests tests/media_tests.cpp)
  target_link_libraries(raymotion_media_tests PRIVATE rmtn_engine)
  add_test(NAME media COMMAND raymotion_media_tests)

  add_executable(raymotion_prefab_tests tests/prefab_tests.cpp)
  target_link_libraries(raymotion_prefab_tests PRIVATE rmtn_engine)
  add_test(NAME prefab COMMAND raymotion_prefab_tests)

  add_executable(raymotion_diagram_render_tests tests/diagram_render_tests.cpp)
  target_link_libraries(raymotion_diagram_render_tests PRIVATE rmtn_engine)
  add_test(NAME diagram_rendering COMMAND raymotion_diagram_render_tests)

  add_executable(raymotion_path_render_tests tests/path_render_tests.cpp)
  target_link_libraries(raymotion_path_render_tests PRIVATE rmtn_engine)
  add_test(NAME path_rendering COMMAND raymotion_path_render_tests)

  # Here-There layouts are designed for Segoe UI: exit 77 (skipped) where it is absent.
  add_executable(raymotion_plain_tests tests/ht_plain_tests.cpp)
  target_link_libraries(raymotion_plain_tests PRIVATE rmtn_engine)
  add_test(NAME ht_plain_layout COMMAND raymotion_plain_tests)

  add_executable(raymotion_feed_tests tests/ht_feed_tests.cpp)
  target_link_libraries(raymotion_feed_tests PRIVATE rmtn_engine)
  target_compile_definitions(raymotion_feed_tests PRIVATE RAYMOTION_FEED_STATE=1 RAYMOTION_FEED_RENDER=1)
  add_test(NAME ht_feed COMMAND raymotion_feed_tests)

  set_tests_properties(renderer_regressions layer_compositing scene3d media diagram_rendering path_rendering ht_plain_layout ht_feed PROPERTIES LABELS gl SKIP_RETURN_CODE 77)

  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(NAME cli_regressions COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.py" "$<TARGET_FILE:raymotion>" "$<TARGET_FILE:raymotion_starter>")
    set_tests_properties(cli_regressions PROPERTIES TIMEOUT 300 LABELS gl)
  endif()
endif()
