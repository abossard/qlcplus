set(module_name "palettegenerator_v5_test")

add_executable(${module_name} WIN32
    ../../../engine/test/common/resource_paths.h
    ${module_name}.cpp ${module_name}.h
    ../../palettegenerator.cpp ../../palettegenerator.h
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

if(qmlui OR (QT_VERSION_MAJOR GREATER 5))
    target_link_libraries(${module_name} PRIVATE
        Qt${QT_MAJOR_VERSION}::Qml
    )
endif()
