project(performfsm_test)

add_executable(${PROJECT_NAME}
    performfsm_test.cpp
    performfsm_test.h
    ../../performfsm.cpp
    ../../performfsm.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/qmlui
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Test
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
