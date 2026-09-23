project(showcommandrecorder_test)

# Link the real UI dependency graph, including the production QML resources.
get_target_property(ui_sources qlcplus5 SOURCES)
get_target_property(ui_source_dir qlcplus5 SOURCE_DIR)
set(test_ui_sources)
foreach(source IN LISTS ui_sources)
    if(source MATCHES "(^|/)main\\.cpp$" OR source MATCHES "\\.qm$")
        continue()
    endif()
    if(NOT IS_ABSOLUTE "${source}")
        set(source "${ui_source_dir}/${source}")
    endif()
    # Generated-source properties are directory-scoped with our CMake policies.
    get_source_file_property(is_generated "${source}" TARGET_DIRECTORY qlcplus5 GENERATED)
    if(is_generated)
        set_source_files_properties("${source}" PROPERTIES GENERATED TRUE)
    endif()
    list(APPEND test_ui_sources "${source}")
endforeach()

add_executable(${PROJECT_NAME}
    showcommandrecorder_test.cpp
    showcommandrecorder_test.h
    ${test_ui_sources}
)
add_dependencies(${PROJECT_NAME} qlcplus5)
target_include_directories(${PROJECT_NAME} PRIVATE
    "$<TARGET_PROPERTY:qlcplus5,INCLUDE_DIRECTORIES>"
    ${ui_source_dir}
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "$<TARGET_PROPERTY:qlcplus5,COMPILE_DEFINITIONS>"
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    "$<TARGET_PROPERTY:qlcplus5,LINK_LIBRARIES>"
    Qt${QT_MAJOR_VERSION}::Test
)
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
set_tests_properties(${PROJECT_NAME} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
