cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(Catnip VERSION 1.0.0)

# debug logging
set(CN_DEBUG ON) 

file(GLOB_RECURSE SOURCES
	src/*.cpp
)

# libraries
set(LIB_LIST ${CMAKE_SOURCE_DIR}/libraries/png/libpng.lib ${CMAKE_SOURCE_DIR}/libraries/zlib/libzlib.lib)

if(CN_DEBUG) 
    # imgui (https://github.com/matcool/CocosExplorer/blob/master/libraries/imgui-hook/CMakeLists.txt)
    add_library(imgui STATIC
        libraries/imgui/imgui.cpp
        libraries/imgui/imgui_widgets.cpp
        libraries/imgui/imgui_tables.cpp
        libraries/imgui/imgui_draw.cpp
        libraries/imgui/backends/imgui_impl_win32.cpp
        libraries/imgui/backends/imgui_impl_opengl3.cpp
        libraries/imgui/misc/cpp/imgui_stdlib.cpp
    )

    set_property(TARGET imgui PROPERTY CXX_STANDARD 17)
    target_include_directories(imgui PUBLIC libraries/imgui)

    list(APPEND LIB_LIST imgui opengl32)
endif()

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC 
    src
    libraries
)

if(CN_DEBUG)
    add_compile_definitions(CN_DEBUG)
endif()

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

target_link_libraries(${PROJECT_NAME} ${LIB_LIST})

setup_geode_mod(${PROJECT_NAME})