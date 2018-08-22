cmake_minimum_required(VERSION 3.8)
project(NanoMesh)

option(NANO_ENABLE_FBX  "Link against FBX Sdk" OFF)
option(NANO_BUILD_TESTS "Build NanoMesh test suite" OFF)

include(mama.cmake)
include_directories(${MAMA_INCLUDES})

if(CLANG OR GCC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC")
endif()

if(NOT NANO_ENABLE_FBX)
    add_definitions(-DNANOMESH_NO_FBX=1)
endif()
if(WIN32)
    if(NANO_ENABLE_FBX)
        set(LIBFBX ${CMAKE_SOURCE_DIR}/lib/libfbxsdk.lib)
    endif()
endif()


include_directories(include)
file(GLOB NANOMESH_INCLUDES include/Nano/*.h)
file(GLOB NANOMESH_SOURCES src/*.cpp src/*.h)
if(WIN32)
    set(NANOMESH_INCLUDES ${NANOMESH_INCLUDES} include/Nano/mesh.natvis)
endif()

add_library(NanoMesh ${NANOMESH_SOURCES})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${NANOMESH_INCLUDES})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${NANOMESH_SOURCES})

target_link_libraries(NanoMesh 
    PRIVATE ${MAMA_LIBS}
    PRIVATE ${LIBFBX})
set_property(TARGET NanoMesh PROPERTY CXX_STANDARD 17)
set_property(TARGET NanoMesh PROPERTY CXX_STANDARD_REQUIRED)

install(TARGETS NanoMesh DESTINATION ${CMAKE_SOURCE_DIR}/lib)

if(NANO_BUILD_TESTS)
    add_subdirectory(test)
endif()
