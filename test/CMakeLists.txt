
file(GLOB_RECURSE NANOMESH_TEST_SOURCES *.cpp *.h)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${NANOMESH_TEST_SOURCES})
add_executable(NanoMeshTests ${NANOMESH_TEST_SOURCES})
target_link_libraries(NanoMeshTests
        NanoMesh
        ${ReCpp_LIBS})
set_property(TARGET NanoMeshTests PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin)


install(TARGETS NanoMeshTests DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../bin)
if(NANO_ENABLE_FBX)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../lib/libfbxsdk.dll
      DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/../bin)
endif()
