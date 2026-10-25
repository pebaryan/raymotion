# A raymotion video project. Build:
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release
cmake_minimum_required(VERSION 3.21)
project(myvideo LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(raymotion
  GIT_REPOSITORY https://github.com/pebaryan/raymotion.git
  GIT_TAG v0.5.0)                # a release tag; newer ones at github.com/pebaryan/raymotion/releases
FetchContent_MakeAvailable(raymotion)

add_executable(myvideo main.cpp)
target_link_libraries(myvideo PRIVATE raymotion::engine)
