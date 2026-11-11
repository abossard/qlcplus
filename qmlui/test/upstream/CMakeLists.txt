# Keep upstream suites separate from the fork's existing suites with the same names.
# Use the production dependency graph rather than upstream's application stubs.
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
    get_source_file_property(is_generated "${source}" TARGET_DIRECTORY qlcplus5 GENERATED)
    if(is_generated)
        set_source_files_properties("${source}" PROPERTIES GENERATED TRUE)
    endif()
    list(APPEND test_ui_sources "${source}")
endforeach()

add_library(qmluitestlib OBJECT ${test_ui_sources})
add_dependencies(qmluitestlib qlcplus5)
target_include_directories(qmluitestlib PUBLIC
    "$<TARGET_PROPERTY:qlcplus5,INCLUDE_DIRECTORIES>"
    ${ui_source_dir}
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)
target_compile_definitions(qmluitestlib PUBLIC
    "$<TARGET_PROPERTY:qlcplus5,COMPILE_DEFINITIONS>"
)
target_link_libraries(qmluitestlib PUBLIC
    "$<TARGET_PROPERTY:qlcplus5,LINK_LIBRARIES>"
    Qt${QT_MAJOR_VERSION}::Test
)

# The retired variable-band audio model is covered by the fork's profile suite.
foreach(suite IN ITEMS vcanimation vcbutton vcclock vccuelist
        vcframe vclabel vcpresets vcslider vcsoloframe vcspeeddial vcwidget
        vcxypad virtualconsole)
    add_executable(upstream_${suite}_test
        ${suite}/${suite}_test.cpp ${suite}/${suite}_test.h)
    target_link_libraries(upstream_${suite}_test PRIVATE qmluitestlib)
    add_test(NAME upstream_${suite}_test COMMAND upstream_${suite}_test)
    set_tests_properties(upstream_${suite}_test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;QLCPLUS_TEST_SETTINGS_PATH=${CMAKE_CURRENT_BINARY_DIR}/settings/${suite}")
endforeach()
