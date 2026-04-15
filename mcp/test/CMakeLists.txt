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
