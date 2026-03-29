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
