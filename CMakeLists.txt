cmake_minimum_required(VERSION 3.20)

# vcpkg integration: pass -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
# at configure time, or set VCPKG_ROOT in your environment as a fallback.
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()

project(APR)

# Set the required C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add packages with vcpkg
find_package(libgit2 CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)

# Create executable
add_executable(apr src/main.cpp src/git2_lib.cpp)
target_sources(apr
  PRIVATE FILE_SET HEADERS
  BASE_DIRS src include
  FILES
    src/Args.hpp
    src/Config.hpp
    src/Core.hpp
    src/git2_lib.hpp
    include/CLI11.hpp
)
target_link_libraries(apr PRIVATE libgit2::libgit2package yaml-cpp::yaml-cpp)
