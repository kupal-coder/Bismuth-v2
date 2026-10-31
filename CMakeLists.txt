cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "iOS" OR IOS)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
else()
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(Bismuth VERSION 1.0.0)

# I want to use `geode build` with clang but this is the only way to do it :(
message(STATUS "Found Geode: $ENV{CMAKE_C_COMPILER}")
if (DEFINED ENV{CMAKE_C_COMPILER})
    set(CMAKE_C_COMPILER $ENV{CMAKE_C_COMPILER})
endif()
if (DEFINED ENV{CMAKE_CXX_COMPILER})
    set(CMAKE_CXX_COMPILER $ENV{CMAKE_CXX_COMPILER})
endif()

# Add all source files inside src (recursively)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

# The desktop renderer needs OpenGL 4.3 (compute shaders, SSBOs, SPIR-V
# specialization). GD on Android creates an ES2 context, so none of that can
# exist there. Android builds use the GLES2 sprite batcher in src/gles2 instead.
if (ANDROID)
    message(STATUS "Bismuth: Android build - excluding desktop GL 4.3 renderer")
    list(FILTER SOURCES EXCLUDE REGEX "/src/(renderer|decomp|culling|math)/")
    list(FILTER SOURCES EXCLUDE REGEX "/src/(gl|cl|profiler|BProfiler|common|ObjectUtils|HotKey|main)\\.cpp$")
else()
    list(FILTER SOURCES EXCLUDE REGEX "/src/gles2/")
endif()

# Set up the mod binary
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Not sure about this...............
set(CMAKE_BUILD_TYPE RelWithDebInfo)
# set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Libraries
set(ENABLE_OPT 0)
set(ENABLE_GLSLANG_BINARIES OFF)

include(FetchContent)

if (NOT ANDROID)
FetchContent_Declare(
	glm
	GIT_REPOSITORY	https://github.com/g-truc/glm.git
	GIT_TAG 	0af55ccecd98d4e5a8d1fad7de25ba429d60e863
)
FetchContent_Declare(glslang GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git GIT_TAG vulkan-sdk-1.4.335)
FetchContent_Declare(
	OpenCL-Headers
	GIT_REPOSITORY	https://github.com/KhronosGroup/OpenCL-Headers.git
	GIT_TAG 	    v2026.05.29
)

FetchContent_MakeAvailable(glm glslang OpenCL-Headers)
endif()

if (NOT ANDROID)
target_link_libraries(${PROJECT_NAME} glm::glm glslang::glslang OpenCL::Headers)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE src)

# Other Geode stuff

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Set up dependencies, resources, and link Geode.
setup_geode_mod(${PROJECT_NAME})
