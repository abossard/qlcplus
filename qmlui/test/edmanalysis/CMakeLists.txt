project(edmanalysis_test)

add_executable(${PROJECT_NAME}
    edmanalysis_test.cpp
    edmanalysis_test.h
    ../../edmanalysisprogress.cpp
    ../../edmanalysisprogress.h
    ../../edmanalysisreader.cpp
    ../../edmanalysisreader.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/qmlui
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/engine/audio/src
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
