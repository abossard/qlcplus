set(module_name "fixturecontrols_test")

add_executable(${module_name} WIN32
    ../../../engine/test/common/resource_paths.h
    ${module_name}.cpp ${module_name}.h
    ../../fixturecontrols.cpp ../../fixturecontrols.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../..
)

target_link_libraries(${module_name} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
