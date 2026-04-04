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
