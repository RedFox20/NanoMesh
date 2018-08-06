cmake_minimum_required(VERSION 3.8)
project(NanoMesh)

include(mama.cmake)
message("MAMA_INCLUDES=${MAMA_INCLUDES}")
include_directories(${MAMA_INCLUDES})

if(WIN32)
    set(RUNTIME opengl32.lib)
elseif(LINUX)
    set(RUNTIME GL X11)
endif()

if(CLANG OR GCC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC")
endif()

file(GLOB NANOMESH_SOURCE Nano/*.cpp Nano/*.h)
if(WIN32)
    set(NANOMESH_SOURCE ${NANOMESH_SOURCE} Nano/mesh.natvis)
endif()

add_library(NanoMesh ${NANOMESH_SOURCE})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${NANOMESH_SOURCE})

target_link_libraries(NanoMesh PRIVATE ${MAMA_LIBS} PRIVATE ${RUNTIME})

set_property(TARGET NanoMesh PROPERTY CXX_STANDARD 17)
set_property(TARGET NanoMesh PROPERTY CXX_STANDARD_REQUIRED)

install(TARGETS NanoMesh DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/lib)
