project(songmanager_test)

add_executable(${PROJECT_NAME}
    songmanager_test.cpp
    songmanager_test.h
    ../../songmanager.cpp
    ../../songmanager.h
    ../../showfactory.cpp
    ../../showfactory.h
    ../../songloadtracker.cpp
    ../../songloadtracker.h
    ../../previewcontext.cpp
    ../../previewcontext.h
    ../../vdjbridge.cpp
    ../../vdjbridge.h
    ../../vdjdeckmodel.cpp
    ../../vdjdeckmodel.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/src
    ${CMAKE_SOURCE_DIR}/engine/audio/src
    ${CMAKE_SOURCE_DIR}/qmlui
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
