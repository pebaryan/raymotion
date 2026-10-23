# Support for the original development machine: a stripped Visual Studio install whose CRT
# headers and import libraries live under Windows Kits paths containing spaces, plus no
# rc/mt tools. Included only when third_party/msvc-sdk-libs holds the mirrored import libs
# (see README "Why the extra vendoring"); a standard toolchain never needs this file.

# CMake's Ninja rules would wrap link with rc/mt manifest steps; that needs a real
# MSVC rc.exe which this machine lacks. Modern Windows doesn't need the embedded
# manifest resource, so neutralize the wrappers.
set(_CMAKE_VS_LINK_EXE "")
set(_CMAKE_VS_LINK_DLL "")

# This machine keeps its C runtime headers as a standalone Windows-Kits component;
# VS's own include tree lacks stdio.h et al. Newest Kits ucrt dir unless -DKIT_INC=...
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
  # Win32 API headers (windows.h etc.), needed when raylib/GLFW build from source.
  foreach(_kit_sub um shared)
    if(EXISTS "${KIT_INC}/../${_kit_sub}")
      get_filename_component(_kit_dir "${KIT_INC}/../${_kit_sub}" ABSOLUTE)
      add_compile_options("/I${_kit_dir}")
    endif()
  endforeach()
else()
  set(CSHIM "${CMAKE_CURRENT_LIST_DIR}/../third_party/c-headers")
  if(EXISTS "${CSHIM}/stdio.h")
    add_compile_options("/I${CSHIM}")   # hand-written fallback if kits headers absent
  endif()
endif()
# <MSVC>/bin/Hostx64/x64/cl.exe -> <MSVC>/include
get_filename_component(_msvc_root "${CMAKE_CXX_COMPILER}" DIRECTORY)
get_filename_component(_msvc_root "${_msvc_root}/../../.." ABSOLUTE)
if(EXISTS "${_msvc_root}/include/vector")
  add_compile_options("/I${_msvc_root}/include")
endif()

# The VS install keeps its import/CRT .libs under directories whose names contain spaces,
# which poison the raw link command line. third_party/msvc-sdk-libs holds the real
# Windows-Kits import archives (um/x64) and third_party/vs-libs the VS runtime archives:
# space-free mirrors, wired in as /LIBPATH search dirs for every target in the build.
get_filename_component(_tp "${CMAKE_CURRENT_LIST_DIR}/../third_party" ABSOLUTE)
set(VSLIBD "${_tp}/vs-libs")
set(KITL "${_tp}/msvc-crt")
foreach(L kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib winmm.lib ddraw.lib)
  if(EXISTS "${_tp}/msvc-sdk-libs/${L}")
    list(APPEND CMAKE_CXX_STANDARD_LIBRARIES "${_tp}/msvc-sdk-libs/${L}")
    list(APPEND CMAKE_C_STANDARD_LIBRARIES   "${_tp}/msvc-sdk-libs/${L}")
  endif()
endforeach()
add_link_options("/LIBPATH:${_tp}/msvc-sdk-libs" "/LIBPATH:${VSLIBD}" "/LIBPATH:${KITL}"
                 "/MANIFEST:NO" "/NODEFAULTLIB" "/ENTRY:mainCRTStartup")
# CRT set pinned explicitly (mirrors above; default resolution can't see space-paths).
# DLL runtime only, matching /MD objects and raylib.lib; never mix in libcmt or ucrtd.
foreach(C "msvcrt.lib" "vcruntime.lib" "msvcprt.lib" "delayimp.lib" "oldnames.lib")
  if(EXISTS "${VSLIBD}/${C}")
    list(APPEND CMAKE_CXX_STANDARD_LIBRARIES "${VSLIBD}/${C}")
    list(APPEND CMAKE_C_STANDARD_LIBRARIES   "${VSLIBD}/${C}")
  endif()
endforeach()
if(EXISTS "${KITL}/ucrt.lib")
  list(APPEND CMAKE_CXX_STANDARD_LIBRARIES "${KITL}/ucrt.lib")
  list(APPEND CMAKE_C_STANDARD_LIBRARIES   "${KITL}/ucrt.lib")
endif()
# single space-joined string; a raw CMake list gets re-joined with ';' on the link line
string(JOIN " " _std_cxx ${CMAKE_CXX_STANDARD_LIBRARIES})
string(JOIN " " _std_c ${CMAKE_C_STANDARD_LIBRARIES})
set(CMAKE_CXX_STANDARD_LIBRARIES "${_std_cxx}")
set(CMAKE_C_STANDARD_LIBRARIES "${_std_c}")
