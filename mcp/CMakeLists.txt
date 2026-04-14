include(FetchContent)
FetchContent_Declare(
    fastmcpp
    GIT_REPOSITORY https://github.com/0xeb/fastmcpp.git
    GIT_TAG main
)
set(FASTMCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FASTMCPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(fastmcpp)

set(MCP_SOURCES
    mcpserver.cpp mcpserver.h
    vcbridge.h
    tools/tool_registry.h
    tools/vc_query_helpers.h
    tools/query_tools.cpp
    tools/function_tools.cpp
    tools/vc_create_tools.cpp
    tools/vc_update_tools.cpp
    tools/vc_input_tools.cpp
    tools/vc_layout_tools.cpp
    tools/io_tools.cpp
    tools/channel_tools.cpp
    tools/palette_tools.cpp
    tools/prompts.cpp
)

add_library(qlcplusmcp STATIC ${MCP_SOURCES})

target_include_directories(qlcplusmcp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/engine/audio/src
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
)

target_link_libraries(qlcplusmcp PUBLIC fastmcpp_core qlcplusengine)
target_link_libraries(qlcplusmcp PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)
target_compile_definitions(qlcplusmcp PUBLIC HAS_MCP_SERVER)
if(qmlui OR (QT_VERSION_MAJOR GREATER 5))
    target_compile_definitions(qlcplusmcp PRIVATE MCP_USE_QJSENGINE)
endif()
if(qmlui)
    target_compile_definitions(qlcplusmcp PRIVATE MCP_SCRIPT_V5)
endif()
target_compile_features(qlcplusmcp PUBLIC cxx_std_17)

add_subdirectory(test)
