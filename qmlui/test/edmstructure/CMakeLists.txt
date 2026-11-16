project(edmstructure_test)

add_executable(${PROJECT_NAME}
    edmstructure_test.cpp
    edmstructure_test.h
    ../../edmstructure.cpp
    ../../edmstructure.h
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

# The looks on real Virtual Console buttons: links the real UI graph, as vcbutton_test does.

get_target_property(edmlooks_ui_sources qlcplus5 SOURCES)
get_target_property(edmlooks_ui_source_dir qlcplus5 SOURCE_DIR)
set(edmlooks_test_ui_sources)
foreach(source IN LISTS edmlooks_ui_sources)
    if(source MATCHES "(^|/)main\\.cpp$" OR source MATCHES "\\.qm$")
        continue()
    endif()
    if(NOT IS_ABSOLUTE "${source}")
        set(source "${edmlooks_ui_source_dir}/${source}")
    endif()
    get_source_file_property(is_generated "${source}" TARGET_DIRECTORY qlcplus5 GENERATED)
    if(is_generated)
        set_source_files_properties("${source}" PROPERTIES GENERATED TRUE)
    endif()
    list(APPEND edmlooks_test_ui_sources "${source}")
endforeach()

add_executable(edmlooks_test
    edmlooks_test.cpp
    edmlooks_test.h
    ${edmlooks_test_ui_sources}
)
add_dependencies(edmlooks_test qlcplus5)
target_include_directories(edmlooks_test PRIVATE
    "$<TARGET_PROPERTY:qlcplus5,INCLUDE_DIRECTORIES>"
    ${edmlooks_ui_source_dir}
)
target_compile_definitions(edmlooks_test PRIVATE
    "$<TARGET_PROPERTY:qlcplus5,COMPILE_DEFINITIONS>"
)
target_link_libraries(edmlooks_test PRIVATE
    "$<TARGET_PROPERTY:qlcplus5,LINK_LIBRARIES>"
    Qt${QT_MAJOR_VERSION}::Test
)
add_test(NAME edmlooks_test COMMAND edmlooks_test)
set_tests_properties(edmlooks_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
