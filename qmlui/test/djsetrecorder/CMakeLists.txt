project(djsetrecorder_test)

add_executable(${PROJECT_NAME}
    djsetrecorder_test.cpp
    djsetrecorder_test.h
    ../../djsetrecorder.cpp
    ../../djsetrecorder.h
    ../../showfactory.cpp
    ../../showfactory.h
    ../../audiobpmtag.cpp
    ../../audiobpmtag.h
    ../../djfsm.cpp
    ../../djfsm.h
    ../../../plugins/vdjbridge/vdjtelemetryclient.cpp
    ../../../plugins/vdjbridge/vdjtelemetryclient.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/engine/audio/src
    ${CMAKE_SOURCE_DIR}/qmlui
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/plugins/vdjbridge
    ${CMAKE_SOURCE_DIR}/qmlui/test/vdjcaptures
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    VDJ_CAPTURE_DIR="${CMAKE_SOURCE_DIR}/qmlui/test/vdjcaptures"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Network
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
    $<$<PLATFORM_ID:Windows>:ws2_32>
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
