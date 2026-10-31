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
  src/engine/lab3d.cpp
  src/engine/smartgrid3d.cpp
  src/engine/interior3d.cpp
  src/engine/computer3d.cpp
  src/engine/aircraft3d.cpp
  src/engine/human3d.cpp
  src/engine/rig.cpp
  src/engine/rig_face.cpp
  src/engine/rig_parts.cpp
  src/engine/rig_act.cpp
  src/engine/rig_creature.cpp
  src/engine/rig_skin.cpp
  src/engine/crowd.cpp
  src/engine/crowd_rig.cpp
  src/engine/rig_ape.cpp
  src/engine/rig_bird.cpp
  src/engine/rig_chain.cpp
  src/engine/rig3d.cpp
  src/engine/rig2d.cpp
  src/engine/particles.cpp
  src/engine/paths.cpp
  src/engine/math_font.cpp
  src/engine/math.cpp
  src/engine/plot.cpp
  src/engine/plot_charts.cpp
  src/engine/plot_diagrams.cpp
  src/engine/space.cpp
  src/engine/space_geometry.cpp
  src/engine/camera.cpp
  src/engine/code.cpp
  src/engine/textfx.cpp
  src/engine/subtitles.cpp
  src/engine/audioviz.cpp
  src/engine/icons.cpp
  src/engine/flags.cpp
  src/engine/geo.cpp
  src/engine/city.cpp
  src/engine/globe.cpp
  src/engine/geo3d.cpp
  src/engine/calendar.cpp)
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
    examples/showcase/ht_explainer_v2.cpp
    examples/showcase/ht_plain.cpp
    examples/showcase/ht_feed.cpp
    examples/showcase/ht_feed_v2.cpp
    examples/showcase/malaria_metabolomics.cpp
    examples/showcase/malaria_metabolomics_v2.cpp
    examples/showcase/a1_sein_six_seats.cpp
    examples/showcase/a1_german_reels.cpp
    examples/showcase/german_cefr_banners.cpp
    examples/showcase/gpu_scene.cpp
    examples/showcase/prefabs.cpp
    examples/showcase/asp.cpp
    examples/showcase/asp_v2.cpp
    examples/showcase/one_walk.cpp
    examples/showcase/trackway.cpp
    examples/showcase/skin_demo.cpp
    examples/showcase/crowd_demo.cpp
    examples/showcase/tokens.cpp
    examples/showcase/own_compute.cpp
    examples/showcase/own_compute_v2.cpp
    examples/showcase/promo.cpp
    examples/showcase/npr.cpp
    examples/showcase/diagrams.cpp
    examples/showcase/particles.cpp
    examples/showcase/particles3d.cpp
    examples/showcase/audioreactive.cpp
    examples/showcase/captions.cpp
    examples/showcase/icons.cpp
    examples/showcase/flags.cpp
    examples/showcase/austria_journey.cpp
    examples/showcase/austria_time_travel.cpp
    examples/showcase/maps.cpp
    examples/showcase/cities.cpp
    examples/showcase/earth.cpp
    examples/showcase/trips.cpp
    examples/showcase/trip_promo.cpp
    examples/showcase/atmosphere.cpp
    examples/showcase/map_charts.cpp
    examples/showcase/paths.cpp
    examples/showcase/equations.cpp
    examples/showcase/plots.cpp
    examples/showcase/charts.cpp
    examples/showcase/spaces.cpp
    examples/showcase/camera.cpp
    examples/showcase/code.cpp
    examples/showcase/text.cpp
    examples/showcase/motionblur.cpp
    examples/showcase/sparql.cpp
    examples/showcase/lowerthird.cpp)
  target_sources(raymotion PRIVATE examples/showcase/expcps_overload.cpp)
  target_sources(raymotion PRIVATE examples/showcase/three_levels_causality.cpp)
  target_link_libraries(raymotion PRIVATE rmtn_engine)

  add_executable(raymotion_halloween examples/halloween/main.cpp)
  target_link_libraries(raymotion_halloween PRIVATE rmtn_engine)

  add_executable(raymotion_halloween3d examples/halloween/main3d.cpp)
  target_link_libraries(raymotion_halloween3d PRIVATE rmtn_engine)

  add_executable(raymotion_starter examples/starter/main.cpp)
  target_link_libraries(raymotion_starter PRIVATE rmtn_engine)

  add_executable(raymotion_kgnesy examples/kgnesy/main.cpp)
  target_link_libraries(raymotion_kgnesy PRIVATE rmtn_engine)

  add_executable(raymotion_reel28 examples/reel28/main.cpp)
  target_link_libraries(raymotion_reel28 PRIVATE rmtn_engine)
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
  add_executable(raymotion_subtitle_tests tests/subtitle_tests.cpp)
  target_link_libraries(raymotion_subtitle_tests PRIVATE rmtn_engine)
  add_test(NAME subtitles COMMAND raymotion_subtitle_tests)
  add_executable(raymotion_icon_tests tests/icon_tests.cpp)
  target_link_libraries(raymotion_icon_tests PRIVATE rmtn_engine)
  add_test(NAME icons COMMAND raymotion_icon_tests)
  add_executable(raymotion_geo_tests tests/geo_tests.cpp)
  target_link_libraries(raymotion_geo_tests PRIVATE rmtn_engine)
  add_test(NAME geo COMMAND raymotion_geo_tests)
  add_executable(raymotion_city_tests tests/city_tests.cpp)
  target_link_libraries(raymotion_city_tests PRIVATE rmtn_engine)
  add_test(NAME city COMMAND raymotion_city_tests)
  add_executable(raymotion_globe_tests tests/globe_tests.cpp)
  target_link_libraries(raymotion_globe_tests PRIVATE rmtn_engine)
  add_test(NAME globe COMMAND raymotion_globe_tests)
  add_executable(raymotion_geo3d_tests tests/geo3d_tests.cpp)
  target_link_libraries(raymotion_geo3d_tests PRIVATE rmtn_engine)
  add_test(NAME geo3d COMMAND raymotion_geo3d_tests)
  add_executable(raymotion_calendar_tests tests/calendar_tests.cpp)
  target_link_libraries(raymotion_calendar_tests PRIVATE rmtn_engine)
  add_test(NAME calendar COMMAND raymotion_calendar_tests)
  add_executable(raymotion_audioviz_tests tests/audioviz_tests.cpp)
  target_link_libraries(raymotion_audioviz_tests PRIVATE rmtn_engine)
  add_test(NAME audioviz COMMAND raymotion_audioviz_tests)
  add_executable(raymotion_path_tests tests/path_tests.cpp)
  target_link_libraries(raymotion_path_tests PRIVATE rmtn_engine)
  add_test(NAME paths COMMAND raymotion_path_tests)
  add_executable(raymotion_math_tests tests/math_tests.cpp)
  target_link_libraries(raymotion_math_tests PRIVATE rmtn_engine)
  add_test(NAME math COMMAND raymotion_math_tests)
  add_executable(raymotion_plot_tests tests/plot_tests.cpp)
  target_link_libraries(raymotion_plot_tests PRIVATE rmtn_engine)
  add_test(NAME plots COMMAND raymotion_plot_tests)

  add_executable(raymotion_space_tests tests/space_tests.cpp)
  target_link_libraries(raymotion_space_tests PRIVATE rmtn_engine)
  add_test(NAME spaces COMMAND raymotion_space_tests)

  add_executable(raymotion_code_tests tests/code_tests.cpp)
  target_link_libraries(raymotion_code_tests PRIVATE rmtn_engine)
  add_test(NAME code COMMAND raymotion_code_tests)

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

  add_executable(raymotion_lab3d_tests tests/lab3d_tests.cpp)
  target_link_libraries(raymotion_lab3d_tests PRIVATE rmtn_engine)
  add_test(NAME lab3d COMMAND raymotion_lab3d_tests)

  add_executable(raymotion_smartgrid3d_tests tests/smartgrid3d_tests.cpp)
  target_link_libraries(raymotion_smartgrid3d_tests PRIVATE rmtn_engine)
  add_test(NAME smartgrid3d COMMAND raymotion_smartgrid3d_tests)

  add_executable(raymotion_interior3d_tests tests/interior3d_tests.cpp)
  target_link_libraries(raymotion_interior3d_tests PRIVATE rmtn_engine)
  add_test(NAME interior3d COMMAND raymotion_interior3d_tests)

  add_executable(raymotion_computer3d_tests tests/computer3d_tests.cpp)
  target_link_libraries(raymotion_computer3d_tests PRIVATE rmtn_engine)
  add_test(NAME computer3d COMMAND raymotion_computer3d_tests)

  add_executable(raymotion_aircraft3d_tests tests/aircraft3d_tests.cpp)
  target_link_libraries(raymotion_aircraft3d_tests PRIVATE rmtn_engine)
  add_test(NAME aircraft3d COMMAND raymotion_aircraft3d_tests)

  add_executable(raymotion_human3d_tests tests/human3d_tests.cpp)
  target_link_libraries(raymotion_human3d_tests PRIVATE rmtn_engine)
  add_test(NAME human3d COMMAND raymotion_human3d_tests)

  add_executable(raymotion_rig_tests tests/rig_tests.cpp)
  target_link_libraries(raymotion_rig_tests PRIVATE rmtn_engine)
  add_test(NAME rig COMMAND raymotion_rig_tests --no-gl)
  add_test(NAME rig_render COMMAND raymotion_rig_tests --gl-only)
  set_tests_properties(rig_render PROPERTIES LABELS gl SKIP_RETURN_CODE 77)
  add_executable(raymotion_crowd_tests tests/crowd_tests.cpp)
  target_link_libraries(raymotion_crowd_tests PRIVATE rmtn_engine)
  add_test(NAME crowd COMMAND raymotion_crowd_tests)

  # A body baked by tools/skin_bake.py, posed by clips (writes stills; not a ctest).
  add_executable(raymotion_skin_view tests/skin_view.cpp)
  target_link_libraries(raymotion_skin_view PRIVATE rmtn_engine)
  add_executable(raymotion_skin_anim tests/skin_anim.cpp)
  target_link_libraries(raymotion_skin_anim PRIVATE rmtn_engine)

  add_executable(raymotion_diagram_render_tests tests/diagram_render_tests.cpp)
  target_link_libraries(raymotion_diagram_render_tests PRIVATE rmtn_engine)
  add_test(NAME diagram_rendering COMMAND raymotion_diagram_render_tests)

  add_executable(raymotion_path_render_tests tests/path_render_tests.cpp)
  target_link_libraries(raymotion_path_render_tests PRIVATE rmtn_engine)
  add_test(NAME path_rendering COMMAND raymotion_path_render_tests)
  add_executable(raymotion_math_render_tests tests/math_render_tests.cpp)
  target_link_libraries(raymotion_math_render_tests PRIVATE rmtn_engine)
  add_test(NAME math_rendering COMMAND raymotion_math_render_tests)
  add_executable(raymotion_plot_render_tests tests/plot_render_tests.cpp)
  target_link_libraries(raymotion_plot_render_tests PRIVATE rmtn_engine)
  add_test(NAME plot_rendering COMMAND raymotion_plot_render_tests)

  add_executable(raymotion_space_render_tests tests/space_render_tests.cpp)
  target_link_libraries(raymotion_space_render_tests PRIVATE rmtn_engine)
  add_test(NAME space_rendering COMMAND raymotion_space_render_tests)

  add_executable(raymotion_code_render_tests tests/code_render_tests.cpp)
  target_link_libraries(raymotion_code_render_tests PRIVATE rmtn_engine)
  add_test(NAME code_rendering COMMAND raymotion_code_render_tests)

  add_executable(raymotion_textfx_tests tests/textfx_tests.cpp)
  target_link_libraries(raymotion_textfx_tests PRIVATE rmtn_engine)
  add_test(NAME text_animation COMMAND raymotion_textfx_tests)

  add_executable(raymotion_motionblur_tests tests/motionblur_tests.cpp)
  target_link_libraries(raymotion_motionblur_tests PRIVATE rmtn_engine)
  add_test(NAME motion_blur COMMAND raymotion_motionblur_tests)

  # Here-There layouts are designed for Segoe UI: exit 77 (skipped) where it is absent.
  add_executable(raymotion_plain_tests tests/ht_plain_tests.cpp)
  target_link_libraries(raymotion_plain_tests PRIVATE rmtn_engine)
  add_test(NAME ht_plain_layout COMMAND raymotion_plain_tests)

  add_executable(raymotion_feed_tests tests/ht_feed_tests.cpp)
  target_link_libraries(raymotion_feed_tests PRIVATE rmtn_engine)
  target_compile_definitions(raymotion_feed_tests PRIVATE RAYMOTION_FEED_STATE=1 RAYMOTION_FEED_RENDER=1)
  add_test(NAME ht_feed COMMAND raymotion_feed_tests)

  set_tests_properties(renderer_regressions layer_compositing scene3d media diagram_rendering path_rendering math_rendering plot_rendering space_rendering code_rendering text_animation motion_blur icons ht_plain_layout ht_feed PROPERTIES LABELS gl SKIP_RETURN_CODE 77)
  # CPU checks first, then GL ones; without a window they exit 77 after the CPU half.
  set_tests_properties(city globe geo3d calendar PROPERTIES SKIP_RETURN_CODE 77)

  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(NAME cli_regressions COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.py" "$<TARGET_FILE:raymotion>" "$<TARGET_FILE:raymotion_starter>")
    set_tests_properties(cli_regressions PROPERTIES TIMEOUT 300 LABELS gl)
  endif()
endif()
