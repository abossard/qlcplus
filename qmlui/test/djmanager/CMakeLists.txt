project(djmanager_test)

add_executable(${PROJECT_NAME}
    djmanager_test.cpp
    djmanager_test.h
    ../../djmanager.cpp
    ../../djmanager.h
    ../../djfsm.cpp
    ../../djfsm.h
    ../../showfactory.cpp
    ../../showfactory.h
    ../../audiobpmtag.cpp
    ../../audiobpmtag.h
    ../../previewcontext.cpp
    ../../previewcontext.h
    ../../vdjbridge.cpp
    ../../vdjbridge.h
    ../../vdjdatabasereader.cpp
    ../../vdjdatabasereader.h
    ../../performfsm.cpp
    ../../performfsm.h
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
