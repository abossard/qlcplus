project(edmstructure_test)

add_executable(${PROJECT_NAME}
    edmstructure_test.cpp
    edmstructure_test.h
    ../../edmstructure.cpp
    ../../edmstructure.h
    ../../edmstructuredirector.cpp
    ../../edmstructuredirector.h
    ../../edmstructuremapping.cpp
    ../../edmstructuremapping.h
    ../../edmanalysisqueue.cpp
    ../../edmanalysisqueue.h
    ../../edmanalysisprogress.cpp
    ../../edmanalysisprogress.h
    ../../edmanalysisreader.cpp
    ../../edmanalysisreader.h
    ../../edmsongposition.cpp
    ../../edmsongposition.h
    ../../djfsm.cpp
    ../../djfsm.h
    ../../../plugins/vdjbridge/vdjtelemetryclient.cpp
    ../../../plugins/vdjbridge/vdjtelemetryclient.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/qmlui
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/engine/audio/src
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
    ${CMAKE_SOURCE_DIR}/plugins/vdjbridge
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Network
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
    $<$<PLATFORM_ID:Windows>:ws2_32>
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
