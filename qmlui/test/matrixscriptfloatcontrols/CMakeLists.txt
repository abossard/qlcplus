project(matrixscriptfloatcontrols_test)

add_executable(${PROJECT_NAME}
    matrixscriptfloatcontrols_test.cpp
    matrixscriptfloatcontrols_test.h
    matrixscriptfloatcontrols_test_stubs.cpp
    ../../functioneditor.cpp
    ../../functioneditor.h
    ../../rgbmatrixeditor.cpp
    ../../rgbmatrixeditor.h
    ../../huematrixeditor.cpp
    ../../huematrixeditor.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/engine/audio/src
    ${CMAKE_SOURCE_DIR}/qmlui
    ${CMAKE_SOURCE_DIR}/qmlui/tardis
    ${CMAKE_SOURCE_DIR}/plugins/interfaces
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
set_tests_properties(${PROJECT_NAME} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
