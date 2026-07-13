cmake_minimum_required(VERSION 3.20)

project(APR)

# Set the required C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Create executable
add_executable(apr)
target_sources(apr PRIVATE main.cpp)
