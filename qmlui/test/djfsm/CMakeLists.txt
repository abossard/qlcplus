project(djfsm_test)

add_executable(${PROJECT_NAME}
    djfsm_test.cpp
    djfsm_test.h
    ../../djfsm.cpp
    ../../djfsm.h
    ../../../plugins/vdjbridge/vdjtelemetryclient.cpp
    ../../../plugins/vdjbridge/vdjtelemetryclient.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/qmlui
    ${CMAKE_SOURCE_DIR}/plugins/vdjbridge
    ${CMAKE_SOURCE_DIR}/qmlui/test/vdjcaptures
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    VDJ_CAPTURE_DIR="${CMAKE_SOURCE_DIR}/qmlui/test/vdjcaptures"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Network
    Qt${QT_MAJOR_VERSION}::Test
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
