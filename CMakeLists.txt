cmake_minimum_required(VERSION 3.15)
project(vectorizer CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(vectorizer
    src/main.cpp
    src/image.cpp
    src/tracer.cpp
    src/rdp.cpp
    src/bezier.cpp
    src/svg.cpp
    src/stb_image_impl.cpp
)

target_include_directories(vectorizer PRIVATE src third_party)

# Synthetic test-image generator used by the scaling experiment
# (see experiments/benchmark.sh).
add_executable(gen_synthetic src/gen_synthetic.cpp)
target_include_directories(gen_synthetic PRIVATE src)

if(MSVC)
    target_compile_options(vectorizer     PRIVATE /W4)
    target_compile_options(gen_synthetic  PRIVATE /W4)
else()
    target_compile_options(vectorizer     PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(gen_synthetic  PRIVATE -Wall -Wextra -Wpedantic)
endif()
