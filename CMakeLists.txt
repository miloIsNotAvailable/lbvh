cmake_minimum_required(VERSION 3.20)

project(peepoo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

# GLAD
# in case:
# in msys2 ucrt 64 terminal run pacman -S mingw-w64-ucrt-x86_64-python-jinja
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        658f48e72aee3c6582e80b05ac0f8787a64fe6bb #v2.0.6
    GIT_PROGRESS   TRUE
    SOURCE_SUBDIR  cmake
)

FetchContent_MakeAvailable(glad)
glad_add_library(glad_gl_core_43 STATIC REPRODUCIBLE LOADER API gl:core=4.3)
FetchContent_MakeAvailable(glfw glad)

FetchContent_Declare(
	glm
	GIT_REPOSITORY	https://github.com/g-truc/glm.git
	GIT_TAG 	0af55ccecd98d4e5a8d1fad7de25ba429d60e863 #refs/tags/1.0.1
)

FetchContent_MakeAvailable(glm)

# All source files in src/
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(${PROJECT_NAME}
    ${SRC_FILES}
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        glfw
        glad_gl_core_43
        glm::glm
)

file(GLOB SRC_HEADER_DIRS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*/headers"
)

target_include_directories(peepoo PRIVATE
    ${SRC_HEADER_DIRS}
)

message(STATUS "SOURCE FILES: ${SRC_HEADER_DIRS}")

# Needed on Linux
if(UNIX AND NOT APPLE)
    # target_link_libraries(${PROJECT_NAME} PRIVATE glad_gl_core_43)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()