add_executable(mcp_idempotency_test WIN32
    idempotency_test.cpp idempotency_test.h
)

target_include_directories(mcp_idempotency_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_idempotency_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)

# VC Validation tests — exercises VCType, VCFields, VCValidate from vc_tools_common.h
add_executable(mcp_vc_validation_test WIN32
    vc_validation_test.cpp vc_validation_test.h
)

target_include_directories(mcp_vc_validation_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_vc_validation_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
    nlohmann_json::nlohmann_json
)

# VC public tool surface tests — registered schemas and stateful behavior
add_executable(mcp_vc_tool_surface_test WIN32
    vc_tool_surface_test.cpp vc_tool_surface_test.h
    ${CMAKE_SOURCE_DIR}/qmlui/virtualconsole/gridlayout.cpp
)

target_include_directories(mcp_vc_tool_surface_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
    ${CMAKE_SOURCE_DIR}/qmlui/virtualconsole
)

target_link_libraries(mcp_vc_tool_surface_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusmcp
    nlohmann_json::nlohmann_json
)

# VC Bridge Structs tests — exercises VCBridge config structs and defaults
add_executable(mcp_vc_structs_test WIN32
    vc_bridge_structs_test.cpp vc_bridge_structs_test.h
)

target_include_directories(mcp_vc_structs_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_vc_structs_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)

# Palette Integration tests — exercises palette CRUD, scene-palette refs, functionToJson with palettes
add_executable(mcp_palette_integration_test WIN32
    palette_integration_test.cpp palette_integration_test.h
)

target_include_directories(mcp_palette_integration_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_palette_integration_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
    nlohmann_json::nlohmann_json
)

# Script Tool tests — exercises JavaScript-only create_scripts MCP tool
add_executable(mcp_script_tool_test WIN32
    script_tool_test.cpp script_tool_test.h
    functionmanager_stub.cpp
)

target_include_directories(mcp_script_tool_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_script_tool_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Widgets
    qlcplusmcp
    qlcplusengine
    nlohmann_json::nlohmann_json
)

# RGB Transform tests — exercises RGBMatrix::applyTransforms rotation and mirroring
add_executable(mcp_rgb_transform_test WIN32
    rgb_transform_test.cpp rgb_transform_test.h
)

target_include_directories(mcp_rgb_transform_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_rgb_transform_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)

# VC Query Filter tests — exercises vc_query_pages filtering, validation, serialization
add_executable(mcp_vc_query_filter_test WIN32
    vc_query_filter_test.cpp vc_query_filter_test.h
)

target_include_directories(mcp_vc_query_filter_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_vc_query_filter_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
    nlohmann_json::nlohmann_json
)

# Query Tool tests — exercises registered query tools and their validation
add_executable(mcp_query_tools_test WIN32
    query_tools_test.cpp query_tools_test.h
)

target_include_directories(mcp_query_tools_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_query_tools_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusmcp
    nlohmann_json::nlohmann_json
)

# Reflow Column Detection tests — exercises overlap-with-tolerance column clustering
add_executable(mcp_reflow_column_test WIN32
    mcp_reflow_column_test.cpp mcp_reflow_column_test.h
)

target_include_directories(mcp_reflow_column_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_reflow_column_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)

# Grid Layout tests — exercises pure grid layout algorithms (compaction, collision resolution)
add_executable(gridlayout_test WIN32
    gridlayout_test.cpp gridlayout_test.h
    ${CMAKE_SOURCE_DIR}/qmlui/virtualconsole/gridlayout.cpp
)

target_include_directories(gridlayout_test PRIVATE
    ${CMAKE_SOURCE_DIR}/qmlui/virtualconsole
)

target_link_libraries(gridlayout_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
)

# UiManager Preset tests — exercises theme preset infrastructure
add_executable(mcp_uimanager_preset_test WIN32
    uimanager_preset_test.cpp uimanager_preset_test.h
    ${CMAKE_SOURCE_DIR}/qmlui/uimanager.cpp
)

target_include_directories(mcp_uimanager_preset_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/qmlui
)

target_link_libraries(mcp_uimanager_preset_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)

# Beat string conversions tests — exercises beatStringToValue / valueToBeatString
add_executable(mcp_conversions_test WIN32
    conversions_test.cpp conversions_test.h
)

target_include_directories(mcp_conversions_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)
target_compile_definitions(mcp_conversions_test PRIVATE
    TIME_UTILS_SOURCE_PATH="${CMAKE_SOURCE_DIR}/qmlui/js/TimeUtils.js")

target_link_libraries(mcp_conversions_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
    nlohmann_json::nlohmann_json
)

# Flow Console tests are integration tests run against the live MCP server.
# See mcp/test/flow_console_integration_test.sh

# Dispatch smoke tests — end-to-end ToolManager dispatch for major tool families
add_executable(mcp_dispatch_smoke_test WIN32
    dispatch_smoke_test.cpp dispatch_smoke_test.h
    functionmanager_stub.cpp
)

target_include_directories(mcp_dispatch_smoke_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_dispatch_smoke_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusmcp
    nlohmann_json::nlohmann_json
)

# VCPage ExternalInputMode tests — exercises string conversion, XML round-trip, PageInfo struct
add_executable(mcp_vcpage_input_mode_test WIN32
    vcpage_input_mode_test.cpp vcpage_input_mode_test.h
)

target_include_directories(mcp_vcpage_input_mode_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
    ${CMAKE_SOURCE_DIR}/qmlui/virtualconsole
)

target_link_libraries(mcp_vcpage_input_mode_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)

# VCRecordPanel tests — DmxCapture, scene/chaser creation, XML round-trip, widget type registration
add_executable(mcp_vcrecordpanel_test WIN32
    vcrecordpanel_test.cpp vcrecordpanel_test.h
)

target_include_directories(mcp_vcrecordpanel_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/qmlui/virtualconsole
)

target_link_libraries(mcp_vcrecordpanel_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)

# IO Tool tests — universe growth via configure_universes and delete_universes
add_executable(mcp_io_tools_test WIN32
    io_tools_test.cpp io_tools_test.h
)

target_include_directories(mcp_io_tools_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_io_tools_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusmcp
    nlohmann_json::nlohmann_json
)

# Delete Tool tests — delete_fixtures, delete_fixture_groups, vc_delete_pages
add_executable(mcp_delete_tools_test WIN32
    delete_tools_test.cpp delete_tools_test.h
    functionmanager_stub.cpp
    ${CMAKE_SOURCE_DIR}/qmlui/virtualconsole/gridlayout.cpp
)

target_include_directories(mcp_delete_tools_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
    ${CMAKE_SOURCE_DIR}/qmlui/virtualconsole
)

target_link_libraries(mcp_delete_tools_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusmcp
    nlohmann_json::nlohmann_json
)

# Workspace Tool tests — save/load/new project file tools over WorkspaceBridge
add_executable(mcp_workspace_tools_test WIN32
    workspace_tools_test.cpp workspace_tools_test.h
)

target_include_directories(mcp_workspace_tools_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_workspace_tools_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusmcp
    nlohmann_json::nlohmann_json
)

add_executable(mcp_stage_tools_test WIN32
    stage_tools_test.cpp stage_tools_test.h
)

target_include_directories(mcp_stage_tools_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_stage_tools_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusmcp
    nlohmann_json::nlohmann_json
)

add_executable(mcp_show_tools_test WIN32
    show_tools_test.cpp show_tools_test.h
)

target_include_directories(mcp_show_tools_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_show_tools_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusmcp
    nlohmann_json::nlohmann_json
)

add_executable(mcp_live_tools_test WIN32
    live_tools_test.cpp live_tools_test.h
)

target_include_directories(mcp_live_tools_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_live_tools_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusmcp
    nlohmann_json::nlohmann_json
)

add_executable(mcp_input_profile_tools_test WIN32
    input_profile_tools_test.cpp input_profile_tools_test.h
)

target_include_directories(mcp_input_profile_tools_test PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/mcp
    ${CMAKE_SOURCE_DIR}/mcp/tools
)

target_link_libraries(mcp_input_profile_tools_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusmcp
    nlohmann_json::nlohmann_json
)
