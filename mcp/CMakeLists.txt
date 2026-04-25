include(FetchContent)
FetchContent_Declare(
    fastmcpp
    GIT_REPOSITORY https://github.com/0xeb/fastmcpp.git
    GIT_TAG main
)
set(FASTMCPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FASTMCPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Suppress -Werror for third-party dependencies (fastmcpp, cpp_httplib)
# which have unused-parameter warnings on MinGW/GCC.
set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)
set(_saved_cxx_flags "${CMAKE_CXX_FLAGS}")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    string(REPLACE "-Werror" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Wno-error")
endif()

FetchContent_MakeAvailable(fastmcpp)

# Restore flags for our own code.
set(CMAKE_CXX_FLAGS "${_saved_cxx_flags}")

set(MCP_SOURCES
    mcpserver.cpp mcpserver.h
    vcbridge.h
    vcbridgev5.cpp vcbridgev5.h
    mcpinit.cpp mcpinit.h
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
    ${CMAKE_SOURCE_DIR}/qmlui/virtualconsole
    ${CMAKE_SOURCE_DIR}/qmlui/tardis
    ${CMAKE_SOURCE_DIR}/qmlui
)

target_link_libraries(qlcplusmcp PUBLIC fastmcpp_core qlcplusengine)

# Treat fastmcpp/cpp_httplib headers as SYSTEM to suppress their warnings
# when included transitively from our .cpp files.
get_target_property(_fastmcpp_inc fastmcpp_core INTERFACE_INCLUDE_DIRECTORIES)
if(_fastmcpp_inc)
    target_include_directories(qlcplusmcp SYSTEM PUBLIC ${_fastmcpp_inc})
endif()

target_link_libraries(qlcplusmcp PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::Widgets
)
# fastmcpp/cpp_httplib uses Winsock on Windows.
if(WIN32)
    target_link_libraries(qlcplusmcp PRIVATE ws2_32)
endif()
target_compile_definitions(qlcplusmcp PUBLIC HAS_MCP_SERVER)
target_compile_features(qlcplusmcp PUBLIC cxx_std_17)

add_subdirectory(test)
