project(vcxypad_test)

add_executable(${PROJECT_NAME}
    vcxypad_test.cpp
    vcxypad_test.h
    vcxypad_test_stubs.cpp
    ../../virtualconsole/vcxypad.cpp
    ../../virtualconsole/vcxypad.h
    ../../virtualconsole/vcxypadpreset.cpp
    ../../virtualconsole/vcxypadpreset.h
    ../../virtualconsole/vcwidget.cpp
    ../../virtualconsole/vcwidget.h
    ../../listmodel.cpp
    ../../listmodel.h
    ../../treemodel.cpp
    ../../treemodel.h
    ../../treemodelitem.cpp
    ../../treemodelitem.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/engine/audio/src
    ${CMAKE_SOURCE_DIR}/qmlui
    ${CMAKE_SOURCE_DIR}/qmlui/tardis
    ${CMAKE_SOURCE_DIR}/qmlui/virtualconsole
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
