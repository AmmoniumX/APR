cmake_minimum_required(VERSION 3.20)

# vcpkg integration: pass -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
# at configure time, or set VCPKG_ROOT in your environment as a fallback.
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()

project(APRI)

# Set the required C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add packages with vcpkg
find_package(libgit2 CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS process)

# libalpm is not in vcpkg; it ships as part of the system pacman package
# (we already depend on the pacman binary being present), so link it via
# pkg-config instead.
find_package(PkgConfig REQUIRED)
pkg_check_modules(ALPM REQUIRED IMPORTED_TARGET libalpm)

# Create executable
add_executable(apri src/main.cpp src/git2_lib.cpp src/Core.cpp src/Config.cpp src/Args.cpp src/pacman_lib.cpp src/srcinfo.cpp)
target_sources(apri
  PRIVATE FILE_SET HEADERS
  BASE_DIRS src include
  FILES
    src/Args.hpp
    src/Config.hpp
    src/Core.hpp
    src/git2_lib.hpp
    src/pacman_lib.hpp
    src/srcinfo.hpp
    src/Logger.hpp
)
target_link_libraries(apri PRIVATE
  libgit2::libgit2package
  yaml-cpp::yaml-cpp
  CLI11::CLI11
  Boost::process
  PkgConfig::ALPM
)
