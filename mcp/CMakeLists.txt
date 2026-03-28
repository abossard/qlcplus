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
    vcbridgev5.cpp vcbridgev5.h
    mcpinit.cpp mcpinit.h
    tools/tool_registry.h
    tools/query_tools.cpp
    tools/function_tools.cpp
    tools/vc_tools.cpp
    tools/io_tools.cpp
    tools/channel_tools.cpp
    tools/prompts.cpp
)

add_library(qlcplusmcp STATIC ${MCP_SOURCES})

target_include_directories(qlcplusmcp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/engine/audio/src
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/qmlui/virtualconsole
    ${CMAKE_SOURCE_DIR}/qmlui
)

target_link_libraries(qlcplusmcp PUBLIC fastmcpp_core qlcplusengine)
target_link_libraries(qlcplusmcp PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::Widgets
)
target_compile_definitions(qlcplusmcp PUBLIC HAS_MCP_SERVER)
target_compile_features(qlcplusmcp PUBLIC cxx_std_17)
