cmake_minimum_required(VERSION 3.8)
project(NanoMesh)

option(NANO_ENABLE_FBX  "Link against FBX Sdk" OFF)
option(NANO_BUILD_TESTS "Build NanoMesh test suite" OFF)

include(mama.cmake)
include_directories(${MAMA_INCLUDES})

if(CLANG OR GCC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -fPIC")
elseif(MSVC)
    # /MD - MultiThreaded Dynamic CRT
    # /wd4996 - Disable deprecation warning       /wd4819 - Disable code page warning (OpenCV)
    # /wd4251 - Disable dll-interface warning     /wd4275 - Disable dll-interface base class warning
    # /W2 - warning level 2  /Zi - Debug Database /Oi - Intrinsics Enabled  /MP - MultiProcess build
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest /Oi /wd4996 /wd4819 /wd4251 /wd4275 /W2 /Zi /MP")
endif()


if(NOT NANO_ENABLE_FBX)
    add_definitions(-DNANOMESH_NO_FBX=1)
endif()
if(WIN32)
    if(NANO_ENABLE_FBX)
        set(LIBFBX ${CMAKE_CURRENT_SOURCE_DIR}/lib/libfbxsdk.lib)
    endif()
endif()


include_directories("include")
file(GLOB_RECURSE NANOMESH_INCLUDES include/*.h)
file(GLOB_RECURSE NANOMESH_SOURCES src/*.cpp src/*.h)
if(MSVC)
    set(NANOMESH_INCLUDES ${NANOMESH_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/include/Nano/NanoMesh.natvis)
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${NANOMESH_INCLUDES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${NANOMESH_SOURCES})

add_library(NanoMesh ${NANOMESH_INCLUDES} ${NANOMESH_SOURCES})
target_link_libraries(NanoMesh
    PRIVATE ${MAMA_LIBS}
    PRIVATE ${LIBFBX})
install(TARGETS NanoMesh DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/lib)

if(NANO_BUILD_TESTS)
    add_subdirectory(test)
endif()
