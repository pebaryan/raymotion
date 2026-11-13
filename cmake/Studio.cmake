# Raymotion Studio: an agentic editor (brief, code, assets, preview, chat, settings) on CUI.
# Opt in with -DRAYMOTION_STUDIO=ON. CUI is fetched at configure time (MIT, C99).
# The studio does not link the renderer; it starts raymotion_script for stills and the live window.
include(FetchContent)
set(CUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CUI_STB_IMAGE ON CACHE BOOL "" FORCE)      # PNG/JPEG for the preview and asset thumbnails
FetchContent_Declare(cui
  GIT_REPOSITORY https://github.com/yappologistic/CUI.git
  GIT_TAG v1.4.0)
FetchContent_MakeAvailable(cui)

add_executable(raymotion_studio
  studio/main.cpp
  studio/agent.cpp
  studio/proc.cpp
  studio/http.cpp
  studio/thumb.cpp)
target_include_directories(raymotion_studio PRIVATE studio)
# rmtn_engine is linked for rmtn::Json and raylib's CPU image functions (gallery thumbnails);
# nothing here opens a raylib window.
target_link_libraries(raymotion_studio PRIVATE cui::cui rmtn_engine)
if(RAYMOTION_SCRIPTING)
  add_dependencies(raymotion_studio raymotion_script)
  target_compile_definitions(raymotion_studio PRIVATE RMTN_STUDIO_HOST="$<TARGET_FILE:raymotion_script>")
endif()
cui_app_target(raymotion_studio)
if(WIN32)
  target_link_libraries(raymotion_studio PRIVATE winhttp)   # the agent's HTTP client; no curl needed
endif()
