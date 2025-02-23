cmake_minimum_required(VERSION 3.21)
project(sdsplash)

# Optimize for size
set(CMAKE_BUILD_TYPE MinSizeRel)

# LTO
# set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)
# set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
# add_definitions(-DNDEBUG=1 -DwxDEBUG_LEVEL=0)

# Main executable
add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/sdsplash.cc)

# target_include_directories(${PROJECT_NAME} PUBLIC /usr/lib/gcc/x86_64-redhat-linux/14/include)
target_include_directories(${PROJECT_NAME} PUBLIC /usr/include/drm)

target_link_libraries(${PROJECT_NAME} drm)
add_subdirectory(${PROJECT_SOURCE_DIR}/rlottie)
target_link_libraries(${PROJECT_NAME} rlottie)

# cmake -B build -G Ninja && cmake --build build
