# raymotion - Remotion-style motion graphics engine on raylib + ffmpeg
cmake_minimum_required(VERSION 3.20)
project(raymotion CXX)

# CMake's Ninja rules would wrap link with rc/mt manifest steps; that needs a real
# MSVC rc.exe which this machine lacks. Modern Windows doesn't need the embedded
# manifest resource, so neutralize the wrappers.
set(_CMAKE_VS_LINK_EXE "")
set(_CMAKE_VS_LINK_DLL "")

set(CMAKE_CXX_STANDARD 17)
# One CRT model everywhere: the prebuilt raylib.lib is /MD (MSVCRT), so everything links
# against the DLL runtime (msvcrt + vcruntime + ucrt + msvcprt).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8 /MP /D_USE_MATH_DEFINES)
  # This machine keeps its C runtime headers as a standalone Windows-Kits component
  # (same source nvcc builds rely on); VS's own include tree lacks stdio.h et al.
  # Newest installed Windows Kits ucrt include dir, unless given via -DKIT_INC=...
  if(NOT KIT_INC)
    file(GLOB _kit_stdio "C:/Program Files (x86)/Windows Kits/10/Include/*/ucrt/stdio.h")
    if(_kit_stdio)
      list(SORT _kit_stdio COMPARE NATURAL ORDER DESCENDING)
      list(GET _kit_stdio 0 _kit_stdio)
      get_filename_component(KIT_INC "${_kit_stdio}" DIRECTORY)
    endif()
  endif()
  if(KIT_INC AND EXISTS "${KIT_INC}/stdio.h")
    add_compile_options("/I${KIT_INC}")
  endif()
  set(CSHIM "${CMAKE_CURRENT_SOURCE_DIR}/third_party/c-headers")
  if(EXISTS "${CSHIM}/stdio.h" AND NOT (KIT_INC AND EXISTS "${KIT_INC}/stdio.h"))
    add_compile_options("/I${CSHIM}")   # hand-written fallback if kits headers absent
  endif()
  # <MSVC>/bin/Hostx64/x64/cl.exe -> <MSVC>/include
  get_filename_component(_msvc_root "${CMAKE_CXX_COMPILER}" DIRECTORY)
  get_filename_component(_msvc_root "${_msvc_root}/../../.." ABSOLUTE)
  set(MSVC_INC "${_msvc_root}/include")
  if(EXISTS "${MSVC_INC}/vector")
    add_compile_options("/I${MSVC_INC}")
  endif()
endif()

add_executable(raymotion src/main.cpp src/scenes.cpp src/fx_present.cpp src/ht_explainer.cpp src/ht_plain.cpp src/ht_feed.cpp)

target_include_directories(raymotion PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib/include)

# Prebuilt static raylib (MSVC x64), vendored under third_party/raylib/lib
target_link_libraries(raymotion PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib/lib/raylib.lib")

# This machine's slim VS install has no Windows-SDK import .lib files and no rc/mt tools.
# Import libs for the core system DLLs are regenerated into third_party/msvc-sdk-libs
# (see tools/mkimports.py); the real ucrt import archive comes from Windows Kits via
# the LIB env search path (same component nvcc/CUDA toolchains use).

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/msvc-sdk-libs/kernel32.lib")
  # This machine's VS install keeps its import/CRT .libs under directories whose names
  # contain spaces, which poison the raw link command line. third_party/msvc-sdk-libs
  # holds the real Windows-Kits import archives (um/x64) and third_party/vs-libs the VS
  # runtime/CRT archives -- both space-free mirrors, wired in as /LIBPATH search dirs so
  # the linker's *default* CRT resolution works exactly like on a normal VS install.
  set(VSLIBD "${CMAKE_CURRENT_SOURCE_DIR}/third_party/vs-libs")
  set(KITL "${CMAKE_CURRENT_SOURCE_DIR}/third_party/msvc-crt")
  foreach(L kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib winmm.lib ddraw.lib)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/msvc-sdk-libs/${L}")
      list(APPEND CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/msvc-sdk-libs/${L}")
      list(APPEND CMAKE_C_STANDARD_LIBRARIES   "${CMAKE_CURRENT_SOURCE_DIR}/third_party/msvc-sdk-libs/${L}")
    endif()
  endforeach()
  target_link_options(raymotion PRIVATE
      "/LIBPATH:${CMAKE_CURRENT_SOURCE_DIR}/third_party/msvc-sdk-libs"
      "/LIBPATH:${VSLIBD}" "/LIBPATH:${KITL}" "/MANIFEST:NO" "/NODEFAULTLIB" "/ENTRY:mainCRTStartup")
  # CRT set pinned explicitly (mirrors above; default resolution can't see space-paths).
  # DLL runtime only, matching /MD objects and raylib.lib; never mix in libcmt or ucrtd.
  foreach(C "msvcrt.lib" "vcruntime.lib" "msvcprt.lib" "delayimp.lib" "oldnames.lib")
    if(EXISTS "${VSLIBD}/${C}")
      list(APPEND CMAKE_CXX_STANDARD_LIBRARIES "${VSLIBD}/${C}")
      list(APPEND CMAKE_C_STANDARD_LIBRARIES   "${VSLIBD}/${C}")
    endif()
  endforeach()
  foreach(C "ucrt.lib")
    if(EXISTS "${KITL}/${C}")
      list(APPEND CMAKE_CXX_STANDARD_LIBRARIES "${KITL}/${C}")
      list(APPEND CMAKE_C_STANDARD_LIBRARIES   "${KITL}/${C}")
    endif()
  endforeach()
  # single space-joined string; a raw CMake list gets re-joined with ';' on the link line
  string(JOIN " " STD_CXX ${CMAKE_CXX_STANDARD_LIBRARIES})
  string(JOIN " " STD_C ${CMAKE_C_STANDARD_LIBRARIES})
  set(CMAKE_CXX_STANDARD_LIBRARIES "${STD_CXX}")
  set(CMAKE_C_STANDARD_LIBRARIES "${STD_C}")
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(raymotion_render_tests tests/render_tests.cpp)
  target_include_directories(raymotion_render_tests PRIVATE third_party/raylib/include)
  target_link_libraries(raymotion_render_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib/lib/raylib.lib")
  get_target_property(RAYMOTION_LINK_OPTIONS raymotion LINK_OPTIONS)
  if(RAYMOTION_LINK_OPTIONS)
    target_link_options(raymotion_render_tests PRIVATE ${RAYMOTION_LINK_OPTIONS})
  endif()
  add_test(NAME renderer_regressions COMMAND raymotion_render_tests)
  add_executable(raymotion_ht_tests tests/ht_logic_tests.cpp)
  if(RAYMOTION_LINK_OPTIONS)
    target_link_options(raymotion_ht_tests PRIVATE ${RAYMOTION_LINK_OPTIONS})
  endif()
  add_test(NAME ht_semantics COMMAND raymotion_ht_tests)
  add_executable(raymotion_plain_tests tests/ht_plain_tests.cpp)
  target_include_directories(raymotion_plain_tests PRIVATE third_party/raylib/include)
  target_link_libraries(raymotion_plain_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib/lib/raylib.lib")
  if(RAYMOTION_LINK_OPTIONS)
    target_link_options(raymotion_plain_tests PRIVATE ${RAYMOTION_LINK_OPTIONS})
  endif()
  add_test(NAME ht_plain_layout COMMAND raymotion_plain_tests)
  add_executable(raymotion_feed_tests tests/ht_feed_tests.cpp)
  target_include_directories(raymotion_feed_tests PRIVATE third_party/raylib/include)
  target_link_libraries(raymotion_feed_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib/lib/raylib.lib")
  if(RAYMOTION_LINK_OPTIONS)
    target_link_options(raymotion_feed_tests PRIVATE ${RAYMOTION_LINK_OPTIONS})
  endif()
  target_compile_definitions(raymotion_feed_tests PRIVATE RAYMOTION_FEED_STATE=1 RAYMOTION_FEED_RENDER=1)
  add_test(NAME ht_feed COMMAND raymotion_feed_tests)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_test(NAME cli_regressions COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.py" "$<TARGET_FILE:raymotion>")
    set_tests_properties(cli_regressions PROPERTIES TIMEOUT 180)
  endif()
endif()
